/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Apache License Usage

Alternatively, this file may be used under the Apache License, Version 2.0 (the 
"Apache License"); you may not use this file except in compliance with the 
Apache License. You may obtain a copy of the Apache License at 
http://www.apache.org/licenses/LICENSE-2.0.

Unless required by applicable law or agreed to in writing, software distributed
under the Apache License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES
OR CONDITIONS OF ANY KIND, either express or implied. See the Apache License for
the specific language governing permissions and limitations under the License.

  Copyright (c) 2026 Audiokinetic Inc.
*******************************************************************************/

#pragma once


#include <AK/SoundEngine/Common/AkEnums.h>
#include <AK/SoundEngine/Common/AkSoundEngineExport.h>
#include <AK/SoundEngine/Common/AkCommandTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Returns the minimum size required to accomodate the specified command and its payload.
/// 
/// \param[in] in_cmd_id ID of command (should be one of the values listed in AkCommand enum)
/// \return Size required to accomodate one instance of this command in the buffer. 0 if ID is unrecognized.
/// \sa AkCommand
AK_EXTERNAPIFUNC(size_t, AK_CommandBuffer_CmdSize)(enum AkCommand in_cmd_id);

/// Returns the minimum size required for a command buffer containing 0 commands.
/// Use this function in conjunction with AK_CommandBuffer_CmdSize to determine the required size to allocate for your needs.
/// \sa AK_CommandBuffer_Init
AK_EXTERNAPIFUNC(size_t, AK_CommandBuffer_MinSize)(void);

/// Allocates and initializes a new command buffer with the specified size using the Wwise SDK's memory manager.
/// The buffer is guaranteed to be aligned on 4 bytes. It is allocated using <tt>AkMemID_Integration</tt> memory category.
/// 
/// Use AK_CommandBuffer_MinSize and AK_CommandBuffer_CmdSize to determine how much space to allocate for the planned buffer.
/// 
/// The buffer returned by this function MUST be freed using AK_CommandBuffer_Destroy.
/// 
/// This function will return NULL under the following error conditions:
/// - Requested size is smaller than the minimium size required for a command buffer.
/// - Memory manager is not initialized.
/// - Not enough memory to allocate the buffer.
/// 
/// \param[in] in_size Size of buffer in bytes
/// \return Pointer to the allocated buffer, NULL on error.
AK_EXTERNAPIFUNC(struct AkCommandBufferHeader *, AK_CommandBuffer_Create)(size_t in_size);

/// Destroy a buffer created by AK_CommandBuffer_Create.
/// When this function returns the buffer is freed and must not be used anymore.
///
/// \akwarning Do not use this function to release a buffer that was not returned by AK_CommandBuffer_Create! Doing so will cause undefined behavior.
/// \endakwarning
/// 
/// \param[in] in_buffer Pointer to the buffer returned by AK_CommandBuffer_Create.
AK_EXTERNAPIFUNC(void, AK_CommandBuffer_Destroy)(void* in_buffer);

/// Initialize a previously-allocated command buffer. Clears any previous data contained in the buffer. A buffer can be re-initialized multiple times with this function.
///
/// Note that it is the responsibility of the caller to allocate the buffer. Use AK_CommandBuffer_MinSize and AK_CommandBuffer_CmdSize to determine how much space to allocate for the planned buffer. 
/// Ensure the allocated buffer is aligned on 4 bytes. 
/// 
/// Alternatively, use AK_CommandBuffer_Create to allocate and initialize the buffer in one call.
/// 
/// \param[out]  out_buffer Pointer to buffer space
/// \param[in]  in_size     Size of buffer in bytes
/// \return Pointer to initialized command buffer header on success, NULL if buffer is smaller than the value returned by AK_CommandBuffer_MinSize or buffer not aligned on 4 bytes.
AK_EXTERNAPIFUNC(struct AkCommandBufferHeader *, AK_CommandBuffer_Init)(void* out_buffer, size_t in_size);

/// Reserve space for a command in the command buffer
/// When this function returns a non-null pointer, the space reserved is zero'ed out. 
/// It is the responsibility of the caller to write the correct payload data in the command's reserved space.
///
/// The buffer must be initialized prior to calling this function, either using AK_CommandBuffer_Create or AK_CommandBuffer_Init. Passing an un-initialized buffer results in undefined behavior.
/// 
/// \param[in,out] in_buffer Pointer to command buffer
/// \param[in]     in_cmd_id ID of command (should be one of the values listed in AkCommand enum)
/// \return Pointer to the corresponding command payload, or NULL if not enough space left in the buffer
/// \sa AkCommand
AK_EXTERNAPIFUNC(void *, AK_CommandBuffer_Add)(void* in_buffer, enum AkCommand in_cmd_id);

/// Calculate the space required to store a null-terminated string in the command buffer.
/// 
/// \param[in] str Null-terminated string.
/// \return Size (in bytes) required to copy the string in the command buffer.
AK_EXTERNAPIFUNC(size_t, AK_CommandBuffer_StringSize)(const char* str);

/// Copy a null-terminated string to the data payload of the last added command.
/// 
/// Some commands, like \c RegisterGameObject, require a string to complete the payload. 
/// First add the command, then call this function to complete the payload. For example:
/// 
///     auto cmd = (AkCmd_RegisterGameObject*)AK_CommandBuffer_Add(buffer, AkCommand_RegisterGameObject);
///     cmd->gameObjectID = 100;
///     AK_CommandBuffer_AddString(buffer, "Player Emitter");
/// 
/// Use AK_CommandBuffer_StringSize to calculate the size required to store the string.
/// 
/// \param[in] in_buffer Pointer to command buffer
/// \param[in] str       Pointer to null-terminated string
/// \return Pointer to the copied string, or NULL if there is not enough space left in the buffer.
AK_EXTERNAPIFUNC(void*, AK_CommandBuffer_AddString)(void* in_buffer, const char * str);

/// Calculate the space required to store an array of arbitrary-sized objects in the command buffer.
/// 
/// \param[in] item_size Size (in bytes) of each element in the array.
/// \param[in] num_items Number of elements in the array.
/// \return Size (in bytes) required to copy the array data in the command buffer.
AK_EXTERNAPIFUNC(size_t, AK_CommandBuffer_ArraySize)(size_t item_size, AkUInt16 num_items);

/// Copy an array of arbitrary-sized objects to the data payload of the last added command.
/// 
/// Some commands, like \c SetListeners, require additional data to complete the payload. 
/// First add the command, then call this function to complete the payload. For example:
/// 
///     auto cmd = (AkCmd_SetListeners*)AK_CommandBuffer_Add(buffer, AkCommand_SetListeners);
///     cmd->gameObjectID = 100;
///     cmd->numListenerIDs = mylistenerArray.size();
///     AK_CommandBuffer_AddArray(buffer, sizeof(AkGameObjectID), mylistenerArray.size(), mylistenerArray.data());
/// 
/// Use AK_CommandBuffer_ArraySize to calculate the size required to store the data.
/// 
/// \param[in] in_buffer Pointer to command buffer
/// \param[in] item_size Size (in bytes) of each element in the array.
/// \param[in] num_items Number of elements in the array.
/// \param[in] items     (optional) Pointer to the first element in the array to copy. If NULL, no copy takes place; array space is merely reserved, pointer to the start of the space is returned, and client is responsible for writing to it.
/// \return Pointer to the copied array, or NULL if there is not enough space left in the buffer.
AK_EXTERNAPIFUNC(void*, AK_CommandBuffer_AddArray)(void* in_buffer, size_t item_size, AkUInt16 num_items, const void* items);

/// Calculate the space required to store an array of external sources.
/// 
/// \param[in] in_uNumSources Number of external sources in the array
/// \param[in] in_sources     Pointer to first element in the external sources array
/// \return Size (in bytes) required to copy the external sources data in the command buffer.
AK_EXTERNAPIFUNC(size_t, AK_CommandBuffer_ExternalSourcesSize)(AkUInt32 in_uNumSources, const struct AkExternalSourceInfo* in_sources);

/// Copy an array of external sources to the data payload of the last added command.
/// 
/// When posting an event using external sources, \c PostEvent command requires additional data to complete the payload. 
/// First add the command, then call this function to complete the payload. For example:
/// 
///     auto cmd = (AkCmd_PostEvent*)AK_CommandBuffer_Add(buffer, AkCommand_PostEvent);
///     // Fill out the command...
///     cmd->numExternalSources = myExternalSourcesArray.size();
///     AK_CommandBuffer_AddExternalSources(buffer, myExternalSourcesArray.size(), myExternalSourcesArray.data());
/// 
/// Use AK_CommandBuffer_ExternalSourcesSize to calculate the size required to store the data.
/// 
/// \param[in] in_buffer      Pointer to command buffer
/// \param[in] in_uNumSources Number of elements in the array.
/// \param[in] in_pSources    Pointer to the first element in the array.
/// \return Pointer to the copied array, or NULL if there is not enough space left in the buffer.
/// \sa AkCmd_SA_PostEvent
AK_EXTERNAPIFUNC(void*, AK_CommandBuffer_AddExternalSources)(void* in_buffer, AkUInt32 in_uNumSources, const struct AkExternalSourceInfo * in_pSources);

/// Calculate the space required to store geometry data in a command buffer.
/// 
/// \param[in] in_geometryParams Pointer to the geometry data.
/// \return Size (in bytes) required to copy the geometry data in the command buffer.
AK_EXTERNAPIFUNC(size_t, AK_CommandBuffer_GeometrySize)(const struct AkGeometryParams* in_geometryParams);

/// Copy geometry data to the data payload of the last added command.
/// 
/// The \c SA_SetGeometry command requires additional data to complete the payload. 
/// First add the command, then call this function to complete the payload. For example:
/// 
///     AkGeometryParams geoParams; // Initialize with valid triangles, vertices, surfaces...
///     auto cmd = (AkCmd_SA_SetGeometry*)AK_CommandBuffer_Add(buffer, AkCommand_SA_SetGeometry);
///     cmd->geometrySetID = myID;
///     AK_CommandBuffer_AddGeometry(buffer, &geoParams);
/// 
/// Use AK_CommandBuffer_GeometrySize to calculate the size required to store the data.
/// 
/// \param[in] in_buffer         Pointer to command buffer
/// \param[in] in_geometryParams Pointer to the geometry data.
/// \return Pointer to the copied data, or NULL if there is not enough space left in the buffer.
/// \sa AkCmd_SA_SetGeometry
AK_EXTERNAPIFUNC(void*, AK_CommandBuffer_AddGeometry)(void* in_buffer, const struct AkGeometryParams* in_geometryParams);

/// Remove the last-added command from the command buffer.
/// This function can be useful when the buffer does not have enough space left for a command's extra data.
/// 
/// For example, using the \c SetListeners command requires adding a variable-size array:
/// 
///     auto cmd = (AkCmd_SetListeners*)AK_CommandBuffer_Add(buffer, AkCommand_SetListeners);
///     cmd->gameObjectID = 100;
///     cmd->numListenerIDs = mylistenerArray.size();
///     if (!AK_CommandBuffer_AddArray(buffer, sizeof(AkGameObjectID), mylistenerArray.size(), mylistenerArray.data()))
///         AK_CommandBuffer_Remove(buffer); // Not enough space for the array, so remove the last command (SetListeners)
/// 
/// This function is a no-op when called on an empty command buffer.
/// 
/// \param[in] in_buffer Pointer to command buffer
AK_EXTERNAPIFUNC(void, AK_CommandBuffer_Remove)(void* in_buffer);

/// Submit the commands contained in a buffer. They will be executed asynchronously on the next audio render.
/// This function blocks until the buffer is submitted, but not until its commands are processed. 
/// Commands are always processed asynchronously. Use the AkCommand_Callback command (AkCmd_Callback) to receive a callback at a specific point during command buffer processing.
/// 
/// \param[in] in_buffer Pointer to command buffer
AK_EXTERNAPIFUNC(void, AK_CommandBuffer_Submit)(void* in_buffer);

/// Submit the commands contained in a buffer. They will be executed asynchronously on the next audio render.
/// This function does not block. If the internal message queue of the audio thread is full, this function returns an error.
/// When this occurs, no command in the buffer has been submitted, and you can safely re-attempt to submit the same buffer at a later time.
/// 
/// \param[in] in_buffer Pointer to command buffer
/// \return
///  - AK_Success on success
///  - AK_Busy if the internal message queue of the audio render thread is full. 
AK_EXTERNAPIFUNC(AKRESULT_t, AK_CommandBuffer_SubmitNonBlocking)(void* in_buffer);

/// Function to begin iteration of commands in a buffer.
/// 
/// \param[in]   in_buffer    Pointer to command buffer
/// \param[out]  out_iterator Pointer to iterator structure to initialize.
/// \sa AK_CommandBuffer_Next
/// \sa AkCommandBufferIterator
AK_EXTERNAPIFUNC(void, AK_CommandBuffer_Begin)(void* in_buffer, struct AkCommandBufferIterator* out_iterator);

/// Function to iterate over the next command in the buffer. 
/// A loop going over all commands in a buffer would be structured as follows:
/// 
///     struct AkCommandBufferIterator it;
///     AK_CommandBuffer_Begin(buffer, &it);
///     while (AK_CommandBuffer_Next(&it)) {
///         printf("Command ID: %d; Command payload: %p\n", it.header->cmd, it.payload);
///     }
/// 
/// \param[in,out] inout_iterator Pointer to iterator initialized with \ref AK_CommandBuffer_Begin
/// \return non-zero when iterator points to the next command, zero if all commands have been iterated over.
/// \sa 
/// - AK_CommandBuffer_Begin
/// - AkCommandBufferIterator
AK_EXTERNAPIFUNC(int, AK_CommandBuffer_Next)(struct AkCommandBufferIterator* inout_iterator);

/// Generates a new playing ID. This is guaranteed to return a different value every time this is called.
AK_EXTERNAPIFUNC(AkPlayingID, AK_SoundEngine_GeneratePlayingID)(void);

/// Universal converter from string to ID for the sound engine.
/// This function will hash the name based on a algorithm ( provided at : /AK/Tools/Common/AkFNVHash.h )
/// Note:
///		This function returns a AkUInt32, which is compatible with:
///		AkUniqueID, AkStateGroupID, AkStateID, AkSwitchGroupID, AkSwitchStateID, AkRtpcID, and so on.
AK_EXTERNAPIFUNC(AkUInt32, AK_SoundEngine_GetIDFromString)(const char* in_pszString);

#ifdef AK_SUPPORT_WCHAR
/// Universal converter from Unicode string to ID for the sound engine.
/// This function will hash the name based on a algorithm ( provided at : /AK/Tools/Common/AkFNVHash.h )
/// Note:
///		This function returns a AkUInt32, which is compatible with:
///		AkUniqueID, AkStateGroupID, AkStateID, AkSwitchGroupID, AkSwitchStateID, AkRtpcID, and so on.
AK_EXTERNAPIFUNC(AkUInt32, AK_SoundEngine_GetIDFromStringW)(const wchar_t* in_pszString);
#endif //AK_SUPPORT_WCHAR

/// Gets the compounded output ID from shareset and device id.
/// Outputs are defined by their type (Audio Device shareset) and their specific system ID.  A system ID could be reused for other device types on some OS or platforms, hence the compounded ID.
/// Use 0 for in_idShareset and in_idDevice to get the Main Output ID (the one usually initialized during AK::SoundEngine::Init)
/// 
/// \param[in] in_idShareset Audio Device ShareSet ID, as defined in the Wwise Project.  If needed, use AK::SoundEngine::GetIDFromString() to convert from a string.  Set to AK_INVALID_UNIQUE_ID to use the default.
/// \param[in] in_idDevice Device specific identifier, when multiple devices of the same type are possible.  If only one device is possible, leave to 0.
/// \return The id of the output
/// 
/// \sa \ref obtaining_device_id
AK_EXTERNAPIFUNC(AkOutputDeviceID, AK_SoundEngine_GetOutputID)(AkUniqueID in_idShareset, AkUInt32 in_idDevice);

#ifdef __cplusplus
} // extern "C"
#endif
