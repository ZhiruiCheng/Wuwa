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

#include <AK/SoundEngine/Common/AkSoundEngineTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Creates an empty array able to hold the number of sources specified by capacity.
/// 
/// \param[in]   capacity    Maximum capacity of array
/// \return Empty array with specified capacity, or NULL on memory allocation failure.
AK_EXTERNAPIFUNC(AkExternalSourceArray, AK_ExternalSourceArray_Create)(AkUInt32 capacity);

/// Creates an array from user data. Data is deep-copied into the new array.
/// 
/// The capacity of the created array is the same as in_uNumSrcs. Therefore, it is not possible to add more sources to this array.
/// 
/// \param[in]   in_uNumSrcs    Number of sources to copy.
/// \param[in]   in_pSources    External source data to copy.
/// \return Array containing the copied data.  
AK_EXTERNAPIFUNC(AkExternalSourceArray, AK_ExternalSourceArray_CreateFromData)(AkUInt32 in_uNumSrcs, struct AkExternalSourceInfo* in_pSources);

/// Add an in-memory external source to this array.
///
/// \param[in]   in_arSources      The array to copy data into.
/// \param[in]   in_codec          Codec ID. See documentation for AkExternalSourceInfo::idCodec.
/// \param[in]   in_cookie         Cookie identifying the source. See documentation for AkExternalSourceInfo::iExternalSrcCookie.
/// \param[in]   in_pInMemory      Pointer to the in-memory file. See documentation for AkExternalSourceInfo::pInMemory.
/// \param[in]   in_uiMemorySize   Size of the data pointed by in_pInMemory.
/// \return 1 if source was added, 0 if array is already filled to capacity.
AK_EXTERNAPIFUNC(int, AK_ExternalSourceArray_AddInMemorySource)(AkExternalSourceArray in_arSources, AkCodecID in_codec, AkUInt32 in_cookie, void* in_pInMemory, AkUInt32 in_uiMemorySize);

/// Add a filename-based external source to this array.
/// 
/// The filename string is copied as well. The client may release the string after calling this function.
///
/// \param[in]   in_arSources      The array to copy data into.
/// \param[in]   in_codec          Codec ID. See documentation for AkExternalSourceInfo::idCodec.
/// \param[in]   in_cookie         Cookie identifying the source. See documentation for AkExternalSourceInfo::iExternalSrcCookie.
/// \param[in]   in_filename       File path for the source. See documentation for AkExternalSourceInfo::szFile.
/// \return 1 if source was added, 0 if array is already filled to capacity or when running out of memory
AK_EXTERNAPIFUNC(int, AK_ExternalSourceArray_AddFileNameSource)(AkExternalSourceArray in_arSources, AkCodecID in_codec, AkUInt32 in_cookie, const char * in_filename);

/// Add a file ID-based external source to this array.
///
/// \param[in]   in_arSources      The array to copy data into.
/// \param[in]   in_codec          Codec ID. See documentation for AkExternalSourceInfo::idCodec.
/// \param[in]   in_cookie         Cookie identifying the source. See documentation for AkExternalSourceInfo::iExternalSrcCookie.
/// \param[in]   in_fileID         ID of the file. See documentation for AkExternalSourceInfo::idFile.
/// \return 1 if source was added, 0 if array is already filled to capacity.
AK_EXTERNAPIFUNC(int, AK_ExternalSourceArray_AddFileIDSource)(AkExternalSourceArray in_arSources, AkCodecID in_codec, AkUInt32 in_cookie, AkFileID in_fileID);

/// Returns number of valid items in the array.
/// 
/// \param[in]   in_arSources      The array
/// \return The number of valid items in the array
AK_EXTERNAPIFUNC(AkUInt32, AK_ExternalSourceArray_Length)(AkExternalSourceArray in_arSources);

/// Returns the capacity (in number of sources) of the array.
/// 
/// \param[in]   in_arSources      The array
/// \return The capacity of the array
AK_EXTERNAPIFUNC(AkUInt32, AK_ExternalSourceArray_Capacity)(AkExternalSourceArray in_arSources);

/// Returns the raw data of the array.
/// 
/// This raw data can be passed to other functions like AK_CommandBuffer_AddExternalSources.
/// 
/// \param[in]   in_arSources      The array
/// \return Pointer to the raw data of the array.
AK_EXTERNAPIFUNC(struct AkExternalSourceInfo*, AK_ExternalSourceArray_Data)(AkExternalSourceArray in_arSources);

/// Destroy an external sources array. This releases the memory held by the array.
/// 
/// After this function returns, in_arSources should be discarded and never used again.
/// 
/// \param[in]   in_arSources      The array to destroy
AK_EXTERNAPIFUNC(void, AK_ExternalSourceArray_Destroy)(AkExternalSourceArray in_arSources);

#ifdef __cplusplus
} // extern "C"
#endif
