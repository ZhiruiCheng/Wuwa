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

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkCallbackTypes.h>
#include <AK/SpatialAudio/Common/AkSpatialAudioTypes.h>

/// IDs for commands that can be sent to the sound engine. Each command has an associated structure that defines its payload.
enum AkCommand
{
	AkCommand_EndOfBuffer = 0,                     ///< Not a real command; used to mark the end of the command buffer. No payload.
	AkCommand_PostEvent,                           ///< See AkCmd_PostEvent
	AkCommand_RegisterGameObject,                  ///< See AkCmd_RegisterGameObject
	AkCommand_UnregisterGameObject,                ///< See AkCmd_UnregisterGameObject
	AkCommand_Callback,                            ///< See AkCmd_Callback
	AkCommand_SetRTPC,                             ///< See AkCmd_SetRTPC
	AkCommand_ResetRTPC,                           ///< See AkCmd_ResetRTPC
	AkCommand_SetPosition,                         ///< See AkCmd_SetPosition
	AkCommand_SetListeners,                        ///< See AkCmd_SetListeners
	AkCommand_SetDefaultListeners,                 ///< See AkCmd_SetDefaultListeners
	AkCommand_ResetListeners,                      ///< See AkCmd_ResetListeners
	AkCommand_SetListenerSpatialization,           ///< See AkCmd_SetListenerSpatialization
	AkCommand_SetGameObjectAuxSendValues,          ///< See AkCmd_SetGameObjectAuxSendValues
	AkCommand_SetGameObjectOutputBusVolume,        ///< See AkCmd_SetGameObjectOutputBusVolume
	AkCommand_SetObjectObstructionAndOcclusion,    ///< See AkCmd_SetObjectObstructionAndOcclusion
	AkCommand_SetMultipleObstructionAndOcclusion,  ///< See AkCmd_SetMultipleObstructionAndOcclusion
	AkCommand_SetScalingFactor,                    ///< See AkCmd_SetScalingFactor
	AkCommand_SetMultiplePositions,                ///< See AkCmd_SetMultiplePositions
	AkCommand_SetDistanceProbe,                    ///< See AkCmd_SetDistanceProbe
	AkCommand_StopAll,                             ///< See AkCmd_StopAll
	AkCommand_ExecuteActionOnEvent,                ///< See AkCmd_ExecuteActionOnEvent
	AkCommand_ExecuteActionOnPlayingID,            ///< See AkCmd_ExecuteActionOnPlayingID
	AkCommand_SeekOnEvent,                         ///< See AkCmd_SeekOnEvent
	AkCommand_SetState,                            ///< See AkCmd_SetState
	AkCommand_SetSwitch,                           ///< See AkCmd_SetSwitch
	AkCommand_PostTrigger,                         ///< See AkCmd_PostTrigger
	AkCommand_PostMIDIOnEvent,                     ///< See AkCmd_PostMIDIOnEvent
	AkCommand_StopMIDIOnEvent,                     ///< See AkCmd_StopMIDIOnEvent

	AkCommand_DynamicSequence_Open,                ///< See AkCmd_DynamicSequence_Open
	AkCommand_DynamicSequence_Op,                  ///< See AkCmd_DynamicSequence_Op
	AkCommand_DynamicSequence_Seek,                ///< See AkCmd_DynamicSequence_Seek

	AkCommand_AddOutput,                           ///< See AkCmd_AddOutput
	AkCommand_RemoveOutput,                        ///< See AkCmd_RemoveOutput
	AkCommand_ReplaceOutput,                       ///< See AkCmd_ReplaceOutput
	AkCommand_SetBusAudioDevice,                   ///< See AkCmd_SetBusAudioDevice
	AkCommand_SetBusConfig,                        ///< See AkCmd_SetBusConfig
	AkCommand_ResetBusConfig,                      ///< See AkCmd_ResetBusConfig
	AkCommand_SetEffect,                           ///< See AkCmd_SetEffect
	AkCommand_SetOutputVolume,                     ///< See AkCmd_SetOutputVolume
	AkCommand_SetPanningRule,                      ///< See AkCmd_SetPanningRule
	AkCommand_SetSpeakerAngles,                    ///< See AkCmd_SetSpeakerAngles
	AkCommand_ControlOutputCapture,                ///< See AkCmd_ControlOutputCapture
	AkCommand_AddOutputCaptureMarker,              ///< See AkCmd_AddOutputCaptureMarker
	AkCommand_ControlOfflineRendering,             ///< See AkCmd_ControlOfflineRendering
	AkCommand_SetRandomSeed,                       ///< See AkCmd_SetRandomSeed
	AkCommand_ControlEventStreamCache,             ///< See AkCmd_ControlEventStreamCache
	AkCommand_ControlSuspendedState,               ///< See AkCmd_ControlSuspendedState
	AkCommand_MuteBackgroundMusic,                 ///< See AkCmd_MuteBackgroundMusic
	AkCommand_SendPluginCustomGameData,            ///< See AkCmd_SendPluginCustomGameData
	AkCommand_SetSidechainMixConfig,               ///< See AkCmd_SetSidechainMixConfig
	AkCommand_ResetGlobalValues,                   ///< See AkCmd_ResetGlobalValues

	AkCommand_SA_Begin,                                 ///< Not a real command ID; defines the start of the SpatialAudio command range
	AkCommand_SA_RegisterListener = AkCommand_SA_Begin, ///< See AkCmd_SA_RegisterListener
	AkCommand_SA_UnregisterListener,                    ///< See AkCmd_SA_UnregisterListener
	AkCommand_SA_SetImageSource,                        ///< See AkCmd_SA_SetImageSource
	AkCommand_SA_RemoveImageSource,                     ///< See AkCmd_SA_RemoveImageSource
	AkCommand_SA_ClearImageSources,                     ///< See AkCmd_SA_ClearImageSources
	AkCommand_SA_SetGeometry,                           ///< See AkCmd_SA_SetGeometry
	AkCommand_SA_RemoveGeometry,                        ///< See AkCmd_SA_RemoveGeometry
	AkCommand_SA_SetGeometryInstance,                   ///< See AkCmd_SA_SetGeometryInstance
	AkCommand_SA_RemoveGeometryInstance,                ///< See AkCmd_SA_RemoveGeometryInstance
	AkCommand_SA_SetRoom,                               ///< See AkCmd_SA_SetRoom
	AkCommand_SA_RemoveRoom,                            ///< See AkCmd_SA_RemoveRoom
	AkCommand_SA_SetPortal,                             ///< See AkCmd_SA_SetPortal
	AkCommand_SA_RemovePortal,                          ///< See AkCmd_SA_RemovePortal
	AkCommand_SA_SetPortalObstructionAndOcclusion,      ///< See AkCmd_SA_SetPortalObstructionAndOcclusion
	AkCommand_SA_SetGameObjectToPortalObstruction,      ///< See AkCmd_SA_SetGameObjectToPortalObstruction
	AkCommand_SA_SetPortalToPortalObstruction,          ///< See AkCmd_SA_SetPortalToPortalObstruction
	AkCommand_SA_SetReverbZone,                         ///< See AkCmd_SA_SetReverbZone
	AkCommand_SA_RemoveReverbZone,                      ///< See AkCmd_SA_RemoveReverbZone
	AkCommand_SA_SetGameObjectInRoom,                   ///< See AkCmd_SA_SetGameObjectInRoom
	AkCommand_SA_UnsetGameObjectInRoom,                 ///< See AkCmd_SA_UnsetGameObjectInRoom
	AkCommand_SA_SetGameObjectRadius,                   ///< See AkCmd_SA_SetGameObjectRadius
	AkCommand_SA_SetAdjacentRoomBleed,                  ///< See AkCmd_SA_SetAdjacentRoomBleed
	AkCommand_SA_SetEarlyReflectionsAuxSend,            ///< See AkCmd_SA_SetEarlyReflectionsAuxSend
	AkCommand_SA_SetEarlyReflectionsVolume,             ///< See AkCmd_SA_SetEarlyReflectionsVolume
	AkCommand_SA_SetReflectionsOrder,                   ///< See AkCmd_SA_SetReflectionsOrder
	AkCommand_SA_SetDiffractionOrder,                   ///< See AkCmd_SA_SetDiffractionOrder
	AkCommand_SA_SetMaxGlobalReflectionPaths,           ///< See AkCmd_SA_SetMaxGlobalReflectionPaths
	AkCommand_SA_SetMaxEmitterRoomAuxSends,             ///< See AkCmd_SA_SetMaxEmitterRoomAuxSends
	AkCommand_SA_SetMaxDiffractionPaths,                ///< See AkCmd_SA_SetMaxDiffractionPaths
	AkCommand_SA_SetSmoothingConstant,                  ///< See AkCmd_SA_SetSmoothingConstant
	AkCommand_SA_SetTransmissionOperation,              ///< See AkCmd_SA_SetTransmissionOperation
	AkCommand_SA_SetNumberOfPrimaryRays,                ///< See AkCmd_SA_SetNumberOfPrimaryRays
	AkCommand_SA_SetLoadBalancingSpread,                ///< See AkCmd_SA_SetLoadBalancingSpread
	AkCommand_SA_ResetStochasticEngine,                 ///< See AkCmd_SA_ResetStochasticEngine

	AkCommand_SA_End,                                   ///< Not a real command ID; defines the end of the SpatialAudio command range

	AkCommand_NUM = AkCommand_SA_End               ///< Indicates number of possible command ID values; not a real command ID
};
typedef AkUInt16 AkCommand_t;

/// Function called when processing the command AkCommand_Callback.
/// \remarks This callback is executed from the audio processing thread. The processing time in the callback function should be minimal. Having too much processing time could result in slowing down the audio processing.
AK_CALLBACK(void, AkCommandCallbackFunc)(void* in_pCookie);

#pragma pack(push, 4)

/// Posts an event. When this command is executed, the actions referenced in the event will be executed.
/// The user is responsible for providing a new unique playingID value for every instance of this command.
/// Re-using the same playing ID will cause actions in this event to be associated with an existing playback.
/// 
/// If \c numExternalSources is set to a value higher than 0, then the client is expected to call AK_CommandBuffer_AddExternalSources after the command:
/// 
///     auto cmd = (AkCmd_PostEvent*)AK_CommandBuffer_Add(buffer, AkCommand_PostEvent);
///     // Fill out the command...
///     cmd->numExternalSources = myExternalSourcesArray.size();
///     AK_CommandBuffer_AddExternalSources(buffer, myExternalSourcesArray.size(), myExternalSourcesArray.data());
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c eventID is not valid or \c numExternalSources is higher than 0 and not enough external sources were added after the command.
/// - AK_InsufficientMemory: Failed to allocate memory necessary to begin processing the command
/// - AK_ResourceInUse: \c playingID was already used in a previous post event command. Use \c AK_SoundEngine_GeneratePlayingID() to ensure that a unique ID is generated for each post event command.
/// - AK_IDNotFound: Event ID not found.
/// - AK_PartialSuccess: When connected to the Wwise Profiler, command has been delayed to a later frame until Wwise synchronizes the event.
/// - AK_MaxReached: Event Cooldown parameters for eventID did not allow the event to play.
/// 
/// \sa
/// - AkCommand_PostEvent
/// - AK_CommandBuffer_AddExternalSources
struct AkCmd_PostEvent
{
	AkPlayingID                  playingID;                 ///<Unique ID that will be associated with this playback. Use \ref AK_SoundEngine_GeneratePlayingID() to generate a new unique playing ID.
	AkUniqueID                   eventID;                   ///<Unique ID of the event
	AkGameObjectID               gameObjectID;              ///<Associated game object ID
	AkPlayingID                  actionTargetPlayingID;     ///<Filters the "Target" of actions in the event by a playing ID. Set to AK_INVALID_PLAYING_ID to disable target filtering. Not all actions are affected by this; see AkActionOnEventType for the list of action types affected by this option.
	AkUInt32                     flags;                     ///<(optional) Bitmask: see \ref AkCallbackType
	AK_WPTR(AkEventCallbackFunc, callback);                 ///<(optional) Callback function
	AK_WPTR(void*,               callbackCookie);           ///<(optional) Callback cookie
	AkUInt32                     numExternalSources;        ///<(optional) Number of elements in \c externalSources array
};

/// Registers a game object ID. 
/// 
/// Optionally, you can associate a name to the game object for profiling purposes. Call AK_CommandBuffer_AddString after adding the command to attach a name to the game object:
/// 
///     auto cmd = (AkCmd_RegisterGameObject*)AK_CommandBuffer_Add(buffer, AkCommand_RegisterGameObject);
///     cmd->gameObjectID = 100;
///     AK_CommandBuffer_AddString(buffer, "Player Emitter");
/// 
/// If the ID was already registered, its name is updated.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID is outside the valid range.
/// - AK_InsufficientMemory: Game object could not be registered due to a memory allocation failure
/// 
/// \sa AkCommand_RegisterGameObject
struct AkCmd_RegisterGameObject
{
	AkGameObjectID       gameObjectID;      ///< ID of the game object to be registered. Valid range is [0 to 0xFFFFFFFFFFFFFFDF].
};

/// Unregisters the game object ID. No-op if the game object ID is not found.
/// AK_INVALID_GAME_OBJECT can be specified to unregister all game objects using a single command.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID is outside the valid range.
/// 
/// \sa AkCommand_UnregisterGameObject
struct AkCmd_UnregisterGameObject
{
	AkGameObjectID gameObjectID;    ///<ID of the game object to be unregistered. Valid range is [0 to 0xFFFFFFFFFFFFFFDF]. Use AK_INVALID_GAME_OBJECT to unregister all game objects.
};

/// When this command is executed, the audio rendering thread will call the specified callback.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c callback is not a valid function pointer.
/// 
/// \sa AkCommand_Callback
struct AkCmd_Callback
{
	AK_WPTR(AkCommandCallbackFunc, callback);
	AK_WPTR(void*, callbackCookie);
};

/// Sets the value of a real-time parameter control.
/// This command may set a game parameter value with global scope, game object scope, or playing ID scope.
/// Playing ID scope supersedes both game object scope and global scope, and game object scope supersedes global scope.
/// (Once a value is set for a specific playing ID, it will not be affected by changes at the game object scope or global
/// scope. Similarly, a value set at the game object scope will not be affected by a global scope change.) Game parameter
/// values set with a global scope are applied to all game objects that not yet registered, or already registered but not
/// overridden with a value with game object or playing id scope. To set a game parameter value with global scope,
/// \c gameObjectID must be set to \c AK_INVALID_GAME_OBJECT \a and \c playingID must be set to \c AK_INVALID_PLAYING_ID.
/// To set a game parameter value with object scope, \c gameObjectID must be set to a valid game object ID \a and 
/// \c playingID must be set to \c AK_INVALID_PLAYING_ID. Finally, to set a game parameter value with playing ID scope,
/// both \c gameObjectID \a and \c playingID must be provided.
/// With this command, you may also change the value of a game parameter over time. To do so, specify a non-zero 
/// value for \c transitionTime. At each audio frame, the game parameter value will be updated internally 
/// according to the interpolation curve.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID specified and is outside the valid range or \c fadeCurve is not a valid AkCurveInterpolation value.
/// - AK_IDNotFound: \c gameObjectID is not a registered game object ID.
/// 
/// \sa AkCommand_SetRTPC
struct AkCmd_SetRTPC
{
	AkRtpcID               rtpcID;                           ///<Game parameter ID
	AkRtpcValue            rtpcValue;                        ///<Value to set
	AkGameObjectID         gameObjectID;                     ///<(optional) Game Object ID; specify AK_INVALID_GAME_OBJECT for global scope or if using playingID
	AkPlayingID            playingID;                        ///<(optional) Playing ID; specify AK_INVALID_PLAYING for global scope or if using gameObjectID
	AkTimeMs               transitionTime;                   ///<(optional) Duration during which the game parameter is interpolated towards in_value, 0 for instant change
	AkUInt16               fadeCurve;                        ///<(optional) Curve type to be used for the game parameter interpolation, see \ref AkCurveInterpolation
	AkUInt16               bypassInternalValueInterpolation; ///<Not 0 if you want to bypass the internal "slew rate" or "over time filtering" specified by the sound designer. This is meant to be used when, for example, loading a level and you don't want the values to interpolate.
};

/// Resets the value of the game parameter to its default value, as specified in the Wwise project.
/// This command may reset a game parameter to its default value with global scope, game object scope, or playing ID scope.
/// Playing ID scope supersedes both game object scope and global scope, and game object scope supersedes global scope.
/// Game parameter values reset with a global scope are applied to all game objects that were not overridden at the game
/// object scope or playing ID scope. To reset a game parameter value with global scope, \c gameObjectID must be set to
/// \c AK_INVALID_GAME_OBJECT \a and \c playingID must be set to \c AK_INVALID_PLAYING_ID.
/// To reset a game parameter value with object scope, \c gameObjectID must be set to a valid game object ID \a and 
/// \c playingID must be set to \c AK_INVALID_PLAYING_ID. Finally, to reset a game parameter value with playing ID scope,
/// both \c gameObjectID \a and \c playingID must be provided.
/// With this command, you may also reset the value of a game parameter over time. To do so, specify a non-zero 
/// value for \c transitionTime. At each audio frame, the game parameter value will be updated internally 
/// according to the interpolation curve.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID specified and is outside the valid range or \c fadeCurve is not a valid AkCurveInterpolation value.
/// - AK_IDNotFound: \c gameObjectID is not a registered game object ID.
/// 
/// \sa AkCommand_ResetRTPC
struct AkCmd_ResetRTPC
{
	AkRtpcID               rtpcID;                           ///<(optional) Game parameter ID; specify AK_INVALID_RTPC_ID to reset all game parameters of a game object
	AkGameObjectID         gameObjectID;                     ///<(optional) Game Object ID; specify AK_INVALID_GAME_OBJECT for global scope or if using playingID
	AkPlayingID            playingID;                        ///<(optional) Playing ID; specify AK_INVALID_PLAYING for global scope or if using gameObjectID
	AkTimeMs               transitionTime;                   ///<(optional) Duration during which the game parameter is interpolated towards in_value, 0 for instant change
	AkUInt16               fadeCurve;                        ///<(optional) Curve type to be used for the game parameter interpolation, see \ref AkCurveInterpolation
	AkUInt16               bypassInternalValueInterpolation; ///<Not 0 if you want to bypass the internal "slew rate" or "over time filtering" specified by the sound designer. This is meant to be used when, for example, loading a level and you don't want the values to interpolate.
};

/// Sets the position of a game object.
/// \warning The object's orientation vectors (position.orientationFront and position.orientationTop) must be normalized.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID is outside the valid range, or position is not a valid transform.
/// - AK_IDNotFound: \c gameObjectID is not a registered game object ID.
/// 
/// \sa
/// - AkCommand_SetPosition
/// - \ref soundengine_3dpositions
struct AkCmd_SetPosition
{
	AkGameObjectID           gameObjectID;    ///<Game Object ID
	struct AkWorldTransform  position;        ///<Game object position
	AkUInt32                 flags;           ///<See \ref AkSetPositionFlags
};

/// Sets multiple positions to a single game object.
/// 
/// Setting multiple positions on a single game object is a way to simulate multiple emission sources while using the resources
/// of only one voice. This can be used to simulate wall openings, area sounds, or multiple objects emitting the same sound in the same area.
/// \aknote
/// - Using this command with only one position is the same as using AkCommand_SetPosition.
/// - If a sound has diffraction enabled, it is treated as AkMultiPositionType_MultiDirections. AkMultiPositionType_MultiSources is not supported in this case.
/// \endaknote
/// 
/// The Sound Engine expects an array of \c AkChannelEmitter objects after the command. For example:
/// 
///     auto cmd = (AkCmd_SetMultiplePositions*)AK_CommandBuffer_Add(buffer, AkCommand_SetMultiplePositions);
///     // Fill out the command...
///     cmd->numPositions = myPositions.size();
///     AK_CommandBuffer_AddArray(buffer, sizeof(AkChannelEmitter), myPositions.size(), myPositions.data());
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID is outside the valid range, or \c flags are invalid, no positions were added after the command, or at least one position in the array is invalid
/// - AK_IDNotFound: \c gameObjectID is not a registered game object ID.
/// 
/// \sa
/// - AkCommand_SetMultiplePositions
/// - AK_CommandBuffer_AddArray
/// - \ref soundengine_3dpositions
/// - \ref soundengine_3dpositions_multiplepos
struct AkCmd_SetMultiplePositions
{
	AkGameObjectID                     gameObjectID;       ///<Game Object ID
	AkUInt32                           flags;              ///<See \ref AkSetPositionFlags
	AkUInt32                           multiPositionType;  ///<See \ref AkMultiPositionType
	AkUInt32                           numPositions;       ///<Number of game positions
};

/// Modifies a game object's set of active listeners. The \c operation field determines the type of operation to
/// execute on the set (set, add, or remove).
/// 
/// The Sound Engine expects an array of \c AkGameObjectID objects after the command. For example:
/// 
///     auto cmd = (AkCmd_SetListeners*)AK_CommandBuffer_Add(buffer, AkCommand_SetListeners);
///     cmd->gameObjectID = 100;
///     cmd->numListenerIDs = mylistenerArray.size();
///     AK_CommandBuffer_AddArray(buffer, sizeof(AkGameObjectID), mylistenerArray.size(), mylistenerArray.data());
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID is outside the valid range, or incomplete array after the command
/// - AK_IDNotFound: \c gameObjectID is not a registered game object ID.
/// 
/// \sa
/// - AkCommand_SetListeners
/// - AK_CommandBuffer_AddArray
/// - \ref soundengine_listeners
struct AkCmd_SetListeners
{
	AkGameObjectID            gameObjectID;   ///<Game Object ID
	AkUInt32                  operation;      ///<Type of operation, see \ref AkListenerOp
	AkUInt32                  numListenerIDs; ///<Number of listeners
};

/// Modifies the default set of associated listeners for game objects that have not explicitly overridden their listener sets.
/// Upon registration, all game objects reference the default listener set. The listeners.operation field determines the type
/// of operation to execute on the set (set, add, or remove).
/// 
/// The Sound Engine expects an array of \c AkGameObjectID objects after the command. For example:
/// 
///     auto cmd = (AkCmd_SetDefaultListeners*)AK_CommandBuffer_Add(buffer, AkCommand_SetDefaultListeners);
///     cmd->numListenerIDs = mylistenerArray.size();
///     AK_CommandBuffer_AddArray(buffer, sizeof(AkGameObjectID), mylistenerArray.size(), mylistenerArray.data());
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID is outside the valid range, or incomplete array after the command
/// 
/// \sa
/// - AkCommand_SetDefaultListeners
/// - AK_CommandBuffer_AddArray
/// - \ref soundengine_listeners
struct AkCmd_SetDefaultListeners
{
	AkUInt32                  operation;      ///<Type of operation, see \ref AkListenerOp
	AkUInt32                  numListenerIDs; ///<Number of listeners
};

/// Resets the listener associations to the default listener(s). This will also reset per-listener gains that may have been set.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID is outside the valid range
/// - AK_IDNotFound: \c gameObjectID is not a registered game object ID.
/// 
/// \sa
/// - AkCommand_ResetListeners
/// - \ref soundengine_listeners
struct AkCmd_ResetListeners
{
	AkGameObjectID  gameObjectID;  ///<Game Object ID
};

/// Sets a listener's spatialization parameters. This lets you define listener-specific 
/// volume offsets for each audio channel.
/// If \c isSpatialized is false, the volumes array is used for this listener (3D positions 
/// have no effect on the speaker distribution). Otherwise, the volumes array is added to the speaker
/// distribution computed for this listener.
/// \remarks
/// - If a sound is mixed into a bus that has a different speaker configuration than \c channelConfig,
/// standard up/downmix rules apply.
/// - Sounds with 3D Spatialization set to None will not be affected by these parameters.
/// 
/// When \c channelConfig is set to a valid channel config, the Sound Engine expects an array of \c AkReal32 objects after the command. 
/// The number of items must correspond to the number of channels in \c channelConfig. For example:
/// 
///		AkReal32 volumes[2] = { 1.0f, 1.0f };
///     auto cmd = (AkCmd_SetListenerSpatialization*)AK_CommandBuffer_Add(buffer, AkCommand_SetListenerSpatialization);
///     cmd->channelConfig.SetStandard(AK_SPEAKER_SETUP_STEREO);
///     AK_CommandBuffer_AddArray(buffer, sizeof(AkReal32), cmd->channelConfig.uNumChannels, volumes);
/// 
/// When \c channelConfig is left to an invalid config, no volume array is expected to follow, and the command acts as a simple spatialization toggle.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c listenerID is outside the valid range, \c channelConfig is invalid, incomplete volumes array after the command
/// - AK_InsufficientMemory: Not enough memory to process the command
/// - AK_IDNotFound: \c gameObjectID is not a registered game object ID.
/// 
/// \sa
/// - AkCommand_SetListenerSpatialization
/// - AK_CommandBuffer_AddArray
/// - \ref soundengine_listeners_spatial
struct AkCmd_SetListenerSpatialization
{
	AkGameObjectID          listenerID;      ///< Listener Game Object ID.
	struct AkChannelConfig  channelConfig;   ///< Channel configuration associated with volumes. When valid, an array of volume offsets is expected to follow the command.
	AkUInt32                isSpatialized;   ///< Spatialization toggle, 0 = not spatialized
};

/// Sets the Auxiliary Busses to route the specified game object. To clear the game object's auxiliary
/// sends, \c numValues must be 0.
/// 
/// The Sound Engine expects an array of \c AkAuxSendValue objects after the command. The number of items must correspond to the
/// number of channels in \c channelConfig. For example:
/// 
///		AkAuxSendValue values[2]; // Initialize array as required
///     auto cmd = (AkCmd_SetGameObjectAuxSendValues*)AK_CommandBuffer_Add(buffer, AkCommand_SetGameObjectAuxSendValues);
///     cmd->numValues = 2;
///     AK_CommandBuffer_AddArray(buffer, sizeof(AkAuxSendValue), cmd->numValues, values);
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID is outside the valid range, or incomplete array after the command.
/// - AK_IDNotFound: \c gameObjectID is not a registered game object ID.
/// 
/// \sa
/// - AkCommand_SetGameObjectAuxSendValues
/// - AK_CommandBuffer_AddArray
/// - \ref soundengine_environments
/// - \ref soundengine_environments_dynamic_aux_bus_routing
/// - \ref soundengine_environments_id_vs_string
struct AkCmd_SetGameObjectAuxSendValues
{
	AkGameObjectID            gameObjectID;  ///< Game Object ID.
	AkUInt32                  numValues;     ///< Number of values.
};

/// Sets the Output Bus Volume (direct) to be used for the specified game object. The control value is a
/// number ranging from 0.0f to 1.0f. Output Bus Volumes are stored per listener association, so executing
/// this command will override the default set of listeners. The game object \c emitterID will now reference
/// its own set of listeners which will be the same as the old set of listeners, but with the new associated
/// gain. Future changes to the default listener set will not be picked up by this game object unless
/// \c AkCommand_ResetListeners is executed.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c emitterID is not a valid game object ID or \c controlValue is outside the valid range.
/// - AK_IDNotFound: \c emitterID is not a registered game object ID.
/// 
/// \sa 
/// - AkCommand_SetGameObjectOutputBusVolume
/// - AkCommand_ResetListeners
/// - \ref soundengine_environments
/// - \ref soundengine_environments_setting_dry_environment
/// - \ref soundengine_environments_id_vs_string
struct AkCmd_SetGameObjectOutputBusVolume
{
	AkGameObjectID            emitterID;    ///<Emitter Game Object ID.
	AkGameObjectID            listenerID;    ///<Listener Game Object ID.
	AkReal32                  controlValue;  ///<A multiplier in the range [0.0f:16.0f] ( -∞ dB to +24 dB).
};

/// Sets the scaling factor of a Game Object. Modifies the attenuation computations on this Game Object to
/// simulate sounds with a larger or smaller area of effect.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID is not a valid game object ID or \c scalingFactor is outside the valid range.
/// - AK_IDNotFound: \c gameObjectID is not a registered game object ID.
/// 
/// \sa AkCommand_SetScalingFactor
struct AkCmd_SetScalingFactor
{
	AkGameObjectID            gameObjectID;   ///<Game Object ID
	AkReal32                  scalingFactor;  ///<Scaling factor
};

/// Sets a game object's obstruction and occlusion levels. If the game object as multiple positions, values are
/// set for all positions. To assign a unique obstruction and occlusion value to each sound position, instead
/// use AkCommand_SetMultipleObstructionAndOcclusion. This function is used to affect how an object should be heard by
/// a specific listener.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c emitterID is not a valid game object ID or obstruction/occlusion values are outside the accepted range
/// - AK_InsufficientMemory: Not enough memory to complete the operation
/// - AK_IDNotFound: \c emitterID is not a registered game object ID.
/// 
/// \sa
/// - AkCommand_SetObjectObstructionAndOcclusion
/// - \ref soundengine_obsocc
/// - \ref soundengine_environments
struct AkCmd_SetObjectObstructionAndOcclusion
{
	AkGameObjectID                      emitterID;   ///<Emitter Game Object ID
	AkGameObjectID                      listenerID;  ///<Listener Game Object ID
	struct AkObstructionOcclusionValues value;       ///<Obstruction and occlusion values
};

/// Sets a game object's obstruction and occlusion value for objects with multiple positions. This command differs
/// from AkCommand_SetObjectObstructionAndOcclusion as a list of obstruction/occlusion pair is provided and each obstruction/occlusion
/// pair will affect the corresponding position defined at the same index.
/// \aknote In the case the number of obstruction/occlusion pairs is smaller than the number of positions, remaining positions' 
/// obstruction/occlusion values are set to 0.0. \endaknote
/// 
/// The Sound Engine expects an array of \c AkObstructionOcclusionValues objects after the command. The number of items must correspond to the
/// value of \c numValues. For example:
/// 
///		AkObstructionOcclusionValues values[2]; // Initialize array as required
///     auto cmd = (AkCmd_SetMultipleObstructionAndOcclusion*)AK_CommandBuffer_Add(buffer, AkCommand_SetMultipleObstructionAndOcclusion);
///     cmd->numValues = 2;
///     AK_CommandBuffer_AddArray(buffer, sizeof(AkObstructionOcclusionValues), cmd->numValues, values);
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c emitterID is not a valid game object ID, incomplete array after the command, or obstruction/occlusion values are outside the accepted range
/// - AK_InsufficientMemory: Not enough memory to complete the operation
/// - AK_IDNotFound: \c emitterID is not a registered game object ID.
/// 
/// \sa
/// - AkCommand_SetMultipleObstructionAndOcclusion
/// - AK_CommandBuffer_AddArray
/// - \ref soundengine_obsocc
/// - \ref soundengine_environments
struct AkCmd_SetMultipleObstructionAndOcclusion
{
	AkGameObjectID                          emitterID;   ///<Emitter Game Object ID
	AkGameObjectID                          listenerID;  ///<Listener Game Object ID
	AkUInt32                                numValues;   ///<Number of values
};

/// Use the position of a separate game object for distance calculations for a specified listener. When this command
/// is executed, Wwise calculates distance attenuation and filtering based on the distance between the distance
/// probe Game Object (\c distanceProbeID) and the emitter Game Object's position. In third-person perspective
/// applications, the distance probe Game Object may be set to the player character's position, and the listener Game
/// Object's position to that of the camera. In this scenario, attenuation is based on the distance between the
/// character and the sound, whereas panning, spatialization, and spread and focus calculations are base on the camera.
/// Both Game Objects, \c gameObjectID and \c distanceProbeID must have been previously registered.
/// To clear the distance probe, and revert to using the listener position for distance calculations, set
/// \c distanceProbeID to \c AK_INVALID_GAME_OBJECT.
/// \aknote If the distance probe Game Object is assigned multiple positions, then the first position is used for
/// distance calculations by the listener. \endaknote
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID is not a valid game object ID.
/// - AK_IDNotFound: \c gameObjectID is not a registered game object or \c distanceProbeID is specified and not a registered game object.
/// - AK_InsufficientMemory: Not enough memory to complete the operation.
/// 
/// \sa AkCommand_SetDistanceProbe
struct AkCmd_SetDistanceProbe
{
	AkGameObjectID  gameObjectID;     ///<Game Object ID
	AkGameObjectID  distanceProbeID;  ///<Probe Game Object ID
};

/// Stops the current content playing associated to the specified game object ID.
/// If no game object is specified, all sounds will be stopped.
/// 
/// This command can fail for the following reasons:
/// - AK_InsufficientMemory: Failed to allocate memory necessary to begin processing the command
/// - AK_IDNotFound: \c gameObjectID was specified but is not a registered game object.
/// 
/// \sa AkCommand_StopAll
struct AkCmd_StopAll
{
	AkGameObjectID gameObjectID;    ///<Game Object ID; set to AK_INVALID_GAME_OBJECT to stop all sounds.
};

/// Executes an action on all nodes that are referenced in the specified event in an action of type play.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c eventID is invalid, \c actionType is not a valid action or \c fadeCurve is not a valid AkCurveInterpolation value.
/// - AK_IDNotFound: Event was not found or \c gameObjectID is specified but not a registered game object.
/// 
/// \sa
/// - <tt>AkActionOnEventType</tt>
/// - <tt>AkCommand_ExecuteActionOnEvent</tt>
struct AkCmd_ExecuteActionOnEvent
{
	AkUniqueID            eventID;        ///< ID of event
	AkGameObjectID        gameObjectID;   ///<(optional) Game Object ID (can be invalid)
	AkTimeMs              transitionTime; ///<(optional) Duration of transition in milliseconds
	AkPlayingID           playingID;      ///<(optional) Limit scope to specified playing ID.
	AkUInt8               fadeCurve;      ///<(optional) Curve type to be used for the game parameter interpolation, see \ref AkCurveInterpolation
	AkUInt8               actionType;     ///< See \ref AkActionOnEventType
};

/// Executes an Action on the content associated to the specified playing ID.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c playingID, \c actionType or \c fadeCurve is invalid.
/// 
/// \sa
/// - <tt>AkActionOnEventType</tt>
/// - <tt>AkCommand_ExecuteActionOnPlayingID</tt>
struct AkCmd_ExecuteActionOnPlayingID
{
	AkUInt32              actionType;     ///< See \ref AkActionOnEventType
	AkPlayingID           playingID;      ///< Playing ID (must be the playing ID for a previously-posted event)
	AkTimeMs              transitionTime; ///<(optional) Duration of transition in milliseconds
	AkUInt8               fadeCurve;      ///<(optional) Curve type to be used for the game parameter interpolation, see \ref AkCurveInterpolation
};

/// Seeks inside all playing objects that are referenced in Play Actions of the specified Event.
///
/// Notes:
///		- This works with all objects of the Containers hierarchy, including Music Segments and Music Switch Containers. 
///		- There is a restriction with sounds that play within a continuous sequence. Seeking is ignored 
///			if one of their ancestors is a continuous (random or sequence) container with crossfade or 
///			trigger rate transitions. Seeking is also ignored with sample-accurate transitions, unless
///			the sound that is currently playing is the first sound of the sequence.
///		- Seeking is also ignored with voices that can go virtual with "From Beginning" behavior. 
///		- Sounds/segments are stopped if <tt>position</tt> is greater than their duration.
///		- <tt>position</tt> is clamped internally to the beginning of the sound/segment.
///		- If the option "Seek to nearest marker" is used, the seeking position snaps to the nearest marker.
///			With objects of the Containers hierarchy, markers are embedded in wave files by an external wave editor.
///			Note that looping regions ("sampler loop") are not considered as markers. Also, the "add file name marker" of the 
///			conversion settings dialog adds a marker at the beginning of the file, which is considered when seeking
///			to nearest marker. In the case of interactive music objects (Music Segments, Music Switch Containers, and Music Playlist Containers), user (wave) markers are ignored:
///			seeking occurs to the nearest segment cue (authored in the segment editor), including the Entry Cue, but excluding the Exit Cue.
///		- This method posts a command in the sound engine queue, thus seeking will not occur before 
///			the audio thread consumes it (after a call to RenderAudio()). 
///
///	Notes specific to Music Segments:
///		- With Music Segments, <tt>position</tt> is relative to the Entry Cue, in milliseconds. Use a negative
///			value to seek within the Pre-Entry.
///		- Music segments cannot be looped. You may want to listen to the AK_EndOfEvent notification 
///			in order to restart them if required.
///		- In order to restart at the correct location, with all their tracks synchronized, Music Segments 
///			take the "look-ahead time" property of their streamed tracks into account for seeking. 
///			Consequently, the resulting position after a call to SeekOnEvent() might be earlier than the 
///			value that was passed to the method. Use <tt>AK::MusicEngine::GetPlayingSegmentInfo()</tt> to query 
///			the exact position of a segment. Also, the segment will be silent during that time
///			(so that it restarts precisely at the position that you specified). <tt>AK::MusicEngine::GetPlayingSegmentInfo()</tt> 
///			also informs you about the remaining look-ahead time.  
///
/// Notes specific to Music Switch Containers:
///		- Seeking triggers a music transition towards the current (or target) segment.
///			This transition is subject to the container's transition rule that matches the current and defined in the container,
///			so the moment when seeking occurs depends on the rule's "Exit At" property. On the other hand, the starting position 
///			in the target segment depends on both the desired seeking position and the rule's "Sync To" property.
///		- If the specified time is greater than the destination segment's length, the modulo is taken.
///
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c eventID is invalid or relative position is not in the valid range.
/// - AK_IDNotFound: Event was not found, or \c gameObjectID is specified but not a registered game object.
/// 
/// \sa
/// - <tt>AkCommand_SeekOnEvent</tt>
/// - <tt>AK::SoundEngine::SeekOnEvent()</tt>
/// - <tt>AK::SoundEngine::GetSourcePlayPosition()</tt>
/// - <tt>AK::MusicEngine::GetPlayingSegmentInfo()</tt>
struct AkCmd_SeekOnEvent
{
	AkUniqueID      eventID;              ///< Unique ID of the event
	union
	{
		AkTimeMs    absolute;             ///< Desired position where playback should restart, in milliseconds
		AkReal32    relative;             ///< Desired position where playback should restart, expressed in a percentage of the file's total duration, between 0 and 1.f (see note above about infinite looping sounds)
	} position;
	AkGameObjectID  gameObjectID;         ///< (optional) Associated game object ID; use AK_INVALID_GAME_OBJECT to affect all game objects
	AkPlayingID     playingID;            ///< (optional) Specify the playing ID for the seek to be applied to. Will result in the seek happening only on active actions of the playing ID. Let it be AK_INVALID_PLAYING_ID to seek on all active actions of this event ID.
	AkUInt8         isPositionRelative;   ///< Determines whether the position should be interpreted as absolute (milliseconds) or relative (%).
	AkUInt8         seekToNearestMarker;  ///< If non-zero, the final seeking position will be made equal to the nearest marker (see note above)
};

/// Sets the state of a State Group (by IDs).
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c stateGroupID is invalid.
/// 
/// \sa 
/// - AkCommand_SetState
/// - AK::SoundEngine::SetState
/// - \ref soundengine_states
struct AkCmd_SetState
{
	AkStateGroupID stateGroupID;    ///< ID of the State Group
	AkStateID      stateID;         ///< ID of the state
};

/// Sets the State of a Switch Group for a particular game object.
/// 
/// This command can fail for the following reason:
/// - AK_InvalidParameter: \c switchGroupID is not valid or \c gameObjectID is not valid
/// - AK_IDNotFound: \c gameObjectID is not a registered game object
/// 
/// \sa 
/// - \ref soundengine_switch
/// - AkCommand_SetSwitch
/// - <tt>AK::SoundEngine::SetSwitch()</tt>
struct AkCmd_SetSwitch
{
	AkSwitchGroupID switchGroupID;  ///< ID of the Switch Group
	AkSwitchStateID switchID;       ///< ID of the Switch
	AkGameObjectID  gameObjectID;   ///< Associated game object ID. Must be specified.
};

/// Post the specified trigger.
/// This operation can be performed at global scope or game object scope.
/// 
/// This command can fail for the following reason:
/// - AK_InvalidParameter: \c triggerID is not valid
/// - AK_IDNotFound: \c gameObjectID is specified but not registered.
/// 
/// \sa 
/// - \ref soundengine_triggers
/// - AkCommand_PostTrigger
/// - <tt>AK::SoundEngine::PostTrigger()</tt>
struct AkCmd_PostTrigger
{
	AkTriggerID    triggerID;     ///< ID of the trigger
	AkGameObjectID gameObjectID;  ///< (optional) Game Object ID; set to AK_INVALID_GAME_OBJECT post trigger in global scope.
};

/// Executes a number of MIDI Events on all nodes that are referenced in the specified Event in an Action of type Play.
/// The time at which a MIDI Event is posted is determined by <tt>is_offset_absolute</tt>. If false, each MIDI event will be
/// posted in <tt>AkMIDIPost::uOffset</tt> samples from the start of the current frame. If true, each MIDI event will be posted
/// at the absolute time <tt>AkMIDIPost::uOffset</tt> samples.
/// To obtain the current absolute time, see <tt>AK::SoundEngine::GetSampleTick</tt>.
/// The duration of a sample can be determined from the sound engine's audio settings, via a call to <tt>AK::SoundEngine::GetAudioSettings</tt>.
/// 
/// The Sound Engine expects an array of \c AkMIDIPost objects after the command. The number of items must correspond to the
/// value of \c numMidiPosts. For example:
/// 
///		AkMIDIPost values[2]; // Initialize array as required
///     auto cmd = (AkCmd_PostMIDIOnEvent*)AK_CommandBuffer_Add(buffer, AkCommand_PostMIDIOnEvent);
///     cmd->numMidiPosts = 2;
///     AK_CommandBuffer_AddArray(buffer, sizeof(AkMIDIPost), cmd->numMidiPosts, values);
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c playingID, \c eventID is an invalid ID, or incomplete array after the command.
/// - AK_InsufficientMemory: Not enough memory to process the command.
/// - AK_IDNotFound: \c eventID not found or \c gameObjectID is specified and not registered, or \c playingID was not found when adding MIDI posts to an existing sequence.
/// - AK_PartialSuccess: When connected to the Wwise Profiler, command has been delayed to a later frame until Wwise synchronizes the event.
/// 
/// \sa
/// - AkCommand_PostMIDIOnEvent
/// - AK_CommandBuffer_AddArray
/// - <tt>AK::SoundEngine::PostMIDIOnEvent</tt>
/// - <tt>AK::SoundEngine::StopMIDIOnEvent</tt>
/// - <tt>AK::SoundEngine::GetAudioSettings</tt>
/// - <tt>AK::SoundEngine::GetSampleTick</tt>
/// - \ref soundengine_midi_event_playing_id
struct AkCmd_PostMIDIOnEvent
{
	AkPlayingID    playingID;                ///< Unique ID that will be associated with this playback. Use \ref AK_SoundEngine_GeneratePlayingID() to generate a new unique playing ID.
	AkUniqueID     eventID;                  ///< Unique ID of the Event
	AkGameObjectID gameObjectID;             ///< Associated game object ID
	AkUInt32       flags;                    ///< Bitmask: see \ref AkCallbackType
	AK_WPTR(AkEventCallbackFunc, callback);  ///<(optional) Callback function
	AK_WPTR(void*, callbackCookie);          ///<(optional) Callback cookie
	AkUInt8        isOffsetAbsolute;         ///< Set to true when AkMIDIPost::uOffset are absolute, false when relative to current frame
	AkUInt8        isNewSequence;            ///< When set, a new sequence identified by <tt>playingID</tt> will be created for these MIDI posts. When unset, MIDI events will be added to an existing sequence specified by <tt>playingID</tt>.
	AkUInt32       numMidiPosts;             ///< Number of MIDI Events to post
};

/// Stops MIDI notes on all nodes that are referenced in the specified event in an action of type Play,
/// with the specified Game Object. Invalid parameters are interpreted as wildcards. For example, calling
/// this function with <tt>eventID</tt> set to AK_INVALID_UNIQUE_ID will stop all MIDI notes for Game Object
/// <tt>gameObjectID</tt>.
/// 
/// \sa
/// - AkCommand_PostMIDIOnEvent
/// - <tt>AK::SoundEngine::StopMIDIOnEvent</tt>
/// - <tt>AK::SoundEngine::PostMIDIOnEvent</tt>
/// - \ref soundengine_midi_event_playing_id
struct AkCmd_StopMIDIOnEvent
{
	AkPlayingID    playingID;       ///< (optional) Target playing ID
	AkUniqueID     eventID;         ///< (optional) Unique ID of the Event
	AkGameObjectID gameObjectID;    ///< (optional) Associated game object ID
};

/// Open a new Dynamic Sequence.
/// 
/// If \c AK_DynamicSequenceSelect bit is set in \c flags, the dynamic sequence uses the callback for determining the next item to play.
/// If this bit is not set, then the dynamic sequence uses the Playlist to determine the next item to play (C++-only API).
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c playingID is an invalid ID or \c type is not a valid dynamic sequence type.
/// - AK_InsufficientMemory: Not enough memory to process the command.
/// - AK_IDNotFound: \c gameObjectID was specified but is not registered.
/// - AK_ResourceInUse: \c playingID was already used for a previous entry.
///
/// \sa
/// - AkCommand_DynamicSequence_Open
/// - AK::SoundEngine::DynamicSequence::Open
struct AkCmd_DynamicSequence_Open
{
	AkPlayingID             playingID;       ///< Unique ID that will be associated with this playback. Use \ref AK_SoundEngine_GeneratePlayingID() to generate a new unique playing ID.
	AkGameObjectID          gameObjectID;    ///< (optional) Associated game object ID
	AkDynamicSequenceType_t type;            ///< Type: see \ref AkDynamicSequenceType
	AkUInt32                flags;           ///< Bitmask: see \ref AkCallbackType
	AK_WPTR(AkEventCallbackFunc, callback);  ///< Callback function
	AK_WPTR(void*, callbackCookie);          ///< Callback cookie
};

/// Execute an operation on a dynamic sequence.
/// 
/// Refer to \ref AkDynamicSequenceOp to learn the possible operations.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c playingID is an invalid ID, or \c operation is not a valid dynamic sequence operation, or \c fadeCurve is not a valid curve type.
/// - AK_IDNotFound: \c playingID is valid but does not refer to an active dynamic sequence
/// 
/// \sa
/// - AkCommand_DynamicSequence_Op
/// - AkCmd_DynamicSequence_Open
/// - AkDynamicSequenceOp
struct AkCmd_DynamicSequence_Op
{
	AkPlayingID            playingID;      ///< Playing ID that was used to open the dynamic sequence
	AkUInt32               operation;      ///< Operation to execute the dynamic sequence
	AkTimeMs               transitionTime; ///<(optional) Duration of transition in milliseconds
	AkCurveInterpolation_t fadeCurve;      ///<(optional) Curve type to be used for the transition, see \ref AkCurveInterpolation
};

/// Seek inside specified Dynamic Sequence.
/// It is only possible to seek in the first item of the sequence.
/// If you seek past the duration of the first item, it will be skipped and an error will reported in the Capture Log and debug output. 
/// All the other items in the sequence will continue to play normally.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c playingID is an invalid ID.
/// - AK_IDNotFound: \c playingID is valid but does not refer to an active dynamic sequence.
/// 
/// \sa
/// - AkCommand_DynamicSequence_Seek
/// - AkCmd_DynamicSequence_Open
/// - AK::SoundEngine::DynamicSequence::Seek
struct AkCmd_DynamicSequence_Seek
{
	AkPlayingID     playingID;            ///< Playing ID that was used to open the dynamic sequence
	union
	{
		AkTimeMs    absolute;             ///< Desired position where playback should restart, in milliseconds
		AkReal32    relative;             ///< Desired position where playback should restart, expressed in a percentage of the file's total duration, between 0 and 1.f (see note above about infinite looping sounds)
	} position;
	AkUInt8         isPositionRelative;   ///< Determines whether the position should be interpreted as absolute (milliseconds) or relative (%).
	AkUInt8         seekToNearestMarker;  ///< If non-zero, the final seeking position will be made equal to the nearest marker (see note above)
};

/// Adds an output to the sound engine. Use this to add controller-attached headphones, controller speakers, DVR output, etc.  
/// The \c settings parameter contains an Audio Device shareset to specify the output plugin to use and a device ID to specify the instance, if necessary (e.g. which game controller).
/// 
/// The Sound Engine can accept an array of \c AkGameObjectID objects after the command. These objects correspond to listener IDs attached to the output device.
/// When specified, only the sounds routed to game objects linked to those listeners will play in this device.
/// It is necessary to have separate listeners if multiple devices of the same type can coexist (e.g. controller speakers).
/// 
/// To attach listener IDs to the device being added:
/// 
///     auto cmd = (AkCmd_AddOutput*)AK_CommandBuffer_Add(buffer, AkCommand_AddOutput);
///     // Fill out the command...
///     cmd->numListenerIDs = mylistenerArray.size();
///     AK_CommandBuffer_AddArray(buffer, sizeof(AkGameObjectID), mylistenerArray.size(), mylistenerArray.data());
/// 
/// When \c numListenerIDs is 0, sound routing simply obey the associations between Master Busses and Audio Devices setup in the Wwise Project.
/// 
/// This command can fail for the following reasons:
/// - \c AK_InvalidParameter: Out of range parameters or unsupported parameter combinations.
/// - \c AK_IDNotFound: The audioDeviceShareset in \c settings doesn't exist.  Possibly, the Init bank isn't loaded yet or was not updated with latest changes.
/// - \c AK_InsufficientMemory: Not enough memory to complete the operation.
/// - \c AK_DeviceNotCompatible: Request output settings are incompatible with current system. A dummy output was created instead.
/// 
/// \sa
/// - AkCommand_AddOutput
/// - AK::SoundEngine::AddOutput
/// - \ref integrating_secondary_outputs
/// - \ref default_audio_devices
struct AkCmd_AddOutput
{
	struct AkOutputSettings settings; ///< Creation parameters for this output. \ref AkOutputSettings
	AkUInt32 numListenerIDs;          ///< The number of listener IDs that follow the command.
};

/// Removes one output added through AkCmd_AddOutput
/// 
/// If a listener was associated with the device, you should consider unregistering the listener before adding this command
/// so that Game Object/Listener routing is properly updated according to your game scenario.
/// 
/// \sa
/// - AkCommand_RemoveOutput
/// - \ref integrating_secondary_outputs
/// - AK::SoundEngine::RemoveOutput
struct AkCmd_RemoveOutput
{
	AkOutputDeviceID outputDeviceID; ///< ID of output device. This can be obtained from \ref AK_SoundEngine_GetOutputID. Set to 0 for primary output.
};

/// Replaces an output device previously created during engine initialization or from AkCmd_AddOutput, with a new output device.
/// In addition to simply removing one output device and adding a new one, the new output device will also be used on all of the master buses
/// that the old output device was associated with, and preserve all listeners that were attached to the old output device.
/// 
/// This command can fail for the following reasons:
/// - \c AK_InvalidParameter: Out of range parameters or unsupported parameter combinations.
/// - \c AK_IDNotFound: The audioDeviceShareset on in_settings doesn't exist.  Possibly, the Init bank isn't loaded yet or was not updated with latest changes.
/// - \c AK_InsufficientMemory: Not enough memory to complete the operation.
/// - \c AK_DeviceNotCompatible: Request output settings are incompatible with current system. A dummy output was created instead.
/// 
/// \sa
/// - AkCommand_ReplaceOutput
/// - AK::SoundEngine::ReplaceOutput
/// - \ref integrating_secondary_outputs
struct AkCmd_ReplaceOutput
{
	AkOutputDeviceID outputDeviceID;    ///< ID of output device to replace. This can be obtained from \ref AK_SoundEngine_GetOutputID. Set to 0 to replace the primary output.
	struct AkOutputSettings settings;   ///< Creation parameters for the new output. \ref AkOutputSettings
};

/// Sets the Audio Device to which a master bus outputs.
/// 
/// This overrides the setting in the Wwise project. Can only be set on top-level busses.
/// 
/// \aknote This command is useful only if used before the creation of an output, at the beginning of the sound engine setup. 
/// Once active outputs using this Bus have been created, it is imperative to use AkCmd_ReplaceOutput instead to change the type of output.
/// \endaknote
/// 
/// This command can fail for the following reasons:
/// - \c AK_InvalidParameter when \c busID or \c outputDeviceID are invalid IDs.
/// - \c AK_IDNotFound when either the Bus ID or the Device ID are not present in the Init bank or when the specified bus is not a top-level bus.
/// 
/// \sa
/// - AkCommand_SetBusAudioDevice
/// - AK::SoundEngine::SetBusDevice
struct AkCmd_SetBusAudioDevice
{
	AkUniqueID busID;                  ///< ID of the master bus
	AkUniqueID audioDeviceSharesetID;  ///< ID of Audio Device shareset to become the new destination output of the master bus.
};

/// Forces channel configuration for the specified bus.
/// 
/// \aknote You cannot change the configuration of the top-level bus.\endaknote
/// 
/// This command can fail for the following reasons:
/// - \c AK_InvalidParameter when \c busID is an invalid ID.
/// - \c AK_IDNotFound when the Bus ID is not present in the Init bank.
/// - \c AK_NotCompatible when the Bus ID refers to a top-level bus.
/// 
/// \sa
/// - AkCommand_SetBusConfig
/// - AK::SoundEngine::SetBusConfig
struct AkCmd_SetBusConfig
{
	AkUniqueID             busID;         ///< Bus Short ID.
	struct AkChannelConfig channelConfig; ///< Desired channel configuration. An invalid configuration (from default constructor) means "as parent".
};

/// Resets the channel configuration for the specified bus.
/// 
/// \aknote You cannot change the configuration of the top-level bus.\endaknote
/// 
/// This command can fail for the following reasons:
/// - \c AK_InvalidParameter when \c busID is an invalid ID.
/// - \c AK_IDNotFound when the Bus ID is not present in the Init bank.
/// - \c AK_NotCompatible when the Bus ID refers to a top-level bus.
/// 
/// \sa
/// - AkCommand_ResetBusConfig
/// - AK::SoundEngine::ResetBusConfig
struct AkCmd_ResetBusConfig
{
	AkUniqueID             busID;         ///< Bus Short ID.
};

/// Resets all global changes made to the sound engine.
/// This includes:
/// - States
/// - RTPCs in the global scope
/// - Changes made on sound object by Event Actions like Set Volume, Set Pitch, etc or equivalent API calls.
/// - Mute/solo status
/// - Effects set dynamically through SetEffect or a Set Effect Action.
/// - Random and Sequence containers histories (last played, etc)
/// \note
/// To reset Game Object specific values, use AK::SoundEngine::UnregisterGameObj or AK::SoundEngine::UnregisterAllGameObj 
/// then AK::SoundEngine::RegisterGameObj if the game object is still needed.
/// \sa
/// - AK::SoundEngine::ResetGlobalValues
struct AkCmd_ResetGlobalValues
{
	AkUInt32 unused;
};

/// Forces channel configuration for the specified Sidechain Mix.
/// 
/// This command can fail for the following reasons:
/// - \c AK_InvalidParameter when \c sidechainMixID is an invalid ID.
/// - \c AK_IDNotFound when the SidechainMix ID is not loaded.
/// 
/// \sa
/// - AkCommand_SetSidechainMixConfig
/// - AK::SoundEngine::SetSidechainMixConfig
struct AkCmd_SetSidechainMixConfig
{
	AkUniqueID             sidechainMixID; ///< SidechainMix Short ID.
	struct AkChannelConfig channelConfig;  ///< Desired channel configuration. An invalid configuration (from default constructor) means "as parent".
};

/// Sets an Effect ShareSet at the specified slot of a container, bus, or output device.
/// 
/// \aknote
/// Replacing effects is preferably done through a Set Effect Event Action.
/// \endaknote
/// 
/// This operation adds a reference on the audio node to an existing ShareSet.
/// 
/// To specify a bus, set \c nodeType to \c AkNodeType_Bus. The bus can be an Audio Bus or an Auxiliary Bus.
/// 
/// To specify an output device, set \c nodeType to \c AkNodeType_AudioDevice. The ID can refer to a specific ID 
/// returned by \c AK::SoundEngine::GetOutputID or 0 to designate the main (default) output device. In this case,
/// only audio device effect sharesets are accepted.
/// 
/// When specifying any other container (e.g. Switch, Blend, Sound SFX, etc.) set \c nodeType to \c AkNodeType_Default.
/// 
/// \aknote This function has unspecified behavior when adding an Effect to a currently playing
/// Bus which does not have any Effects, or removing the last Effect on a currently playing bus.
/// \endaknote
/// 
/// \aknote This function will replace existing Effects on the node. If the target node is not at 
/// the top of the hierarchy and is in the Containers hierarchy, the option "Override Parent" in 
/// the Effect section in Wwise must be enabled for this node, otherwise the parent's Effect will 
/// still be the one in use and SetEffect will have no impact.
/// \endaknote
///
/// This command can fail for the following reasons:
/// - \c AK_InvalidParameter when \c nodeID is an invalid ID, or when \c fxIndex or \c nodeType is out of range.
/// - \c AK_IDNotFound when either the Node ID or the Shareset ID are not present in the Init bank.
/// - \c AK_RenderedFX when the current effect in the slot is rendered at bank-generation time (rendered effects cannot be replaced).
/// - \c AK_NotCompatible when \c nodeType is \c AkNodeType_AudioDevice and the effect shareset is not an output device effect.
/// - \c AK_InsufficientMemory when there is not enough memory to complete the operation.
/// 
/// \sa
/// - AkCommand_SetEffect
/// - AK::SoundEngine::SetActorMixerEffect
/// - AK::SoundEngine::SetBusEffect
/// - AK::SoundEngine::SetOutputDeviceEffect
struct AkCmd_SetEffect
{
	AkUInt64   nodeID;       ///< ID of the bus, Container node, or output device. When \c nodeType is set to \c AkNodeType_AudioDevice, a value of 0 designates the main (default) output.
	AkUInt8    nodeType;     ///< Type of node that \c nodeID refers to. Must be a valid value from the \c AkNodeType enumeration.
	AkUInt8    fxIndex;      ///< Effect slot index (0-254)
	AkUniqueID fxSharesetID; ///< ShareSet ID of the effect; pass AK_INVALID_UNIQUE_ID to clear the Effect slot
};

/// Sets the volume of a output device.
///
/// This command will fail if the value specified was NaN or Inf.
struct AkCmd_SetOutputVolume
{
	AkOutputDeviceID outputID; ///< Output ID to set the volume on. This can be obtained from \ref AK_SoundEngine_GetOutputID. Set to 0 for primary output.
	AkReal32         volume;   ///< Volume (0.0 = Muted, 1.0 = Volume max).
};

/// Sets the panning rule of the specified output.
/// 
/// This may be changed anytime once the sound engine is initialized.
/// 
/// \aknote
///	The specified panning rule will only impact the sound if the processing format is downmixing to Stereo in the mixing process. It
/// will not impact the output if the audio stays in 5.1 until the end, for example.
/// \endaknote
/// 
/// This command can fail for the following reasons:
/// - \c AK_InvalidParameter when panningRule is not one of the values in the enum \c AkPanningRule.
/// 
/// \sa
/// - AkCommand_SetPanningRule
/// - AK::SoundEngine::SetPanningRule
struct AkCmd_SetPanningRule
{
	AkUInt32         panningRule; ///< Panning rule.
	AkOutputDeviceID outputID;    ///< Output ID to set the panning rule on. This can be obtained from \ref AK_SoundEngine_GetOutputID. Set to 0 for primary output.
};

/// Sets speaker angles of the specified device. Speaker angles are used for 3D positioning of sounds over standard configurations.
/// Note that the current version of Wwise only supports positioning on the plane.
///
/// The Sound Engine expects an array of loudspeaker pair angles  after the command (in degrees relative to azimuth ]0,180]). For example:
///
///     auto cmd = (AkCmd_SetSpeakerAngles*)AK_CommandBuffer_Add(buffer, AkCommand_SetSpeakerAngles);
///     cmd->idOutput = 0;
///     cmd->fHeightAngle = 30.0f;
///     cmd->uNumAngles = myAngles.size();
///     AK_CommandBuffer_AddArray(buffer, sizeof(AkReal32), myAngles.size(), myAngles.data());
///
/// The speaker angles are expressed as an array of loudspeaker pairs, in degrees, relative to azimuth ]0,180], for a 7.1 speaker configuration.
/// Supported loudspeaker setups are always symmetric; the center speaker is always in the middle and thus not specified by angles.
/// Angles must be set in ascending order.
/// \aknote
/// - This command requires the minimum speaker angle between any pair of speakers to be at least 5 degrees.
/// - When setting angles for a 5.1 speaker layout, we recommend that you select an angle for the SL and SR channels, then subtract 15 degrees for in_pfSpeakerAngles[1] and add 15 degrees for in_pfSpeakerAngles[2] to set the arc appropriately.
/// \endaknote
///
/// Typical usage:
/// - Initialize the sound engine and/or add secondary output(s).
/// - Get number of speaker angles and their value into an array using GetSpeakerAngles().
/// - Modify the angles and call SetSpeakerAngles().
/// \warning This command only applies to configurations (or subset of these configurations) that are standard and whose speakers are on the plane (2D).
///
/// This command can fail for the following reasons:
/// - \c AK_InvalidParameter when one of the angle values is NaN or Inf, or if one of the parameter is invalid. Check the debug log.
/// - \c AK_IDNotFound if \c outputID refers to a non-existent device
/// - \c AK_InsufficientMemory if there is not enough memory to complete the operation.
/// 
/// \sa
/// - AkCommand_SetSpeakerAngles
/// - AK::SoundEngine::SetSpeakerAngles
struct AkCmd_SetSpeakerAngles
{
	AkUInt32         numAngles;   ///< Number of elements in in_pfSpeakerAngles. It must correspond to AK::GetNumberOfAnglesForConfig( AK_SPEAKER_SETUP_DEFAULT_PLANE ) (the value returned by GetSpeakerAngles()).
	AkReal32         heightAngle; ///< Elevation of the height layer, in degrees relative to the plane [-90,90], but it cannot be 0.
	AkOutputDeviceID outputID;    ///< Output ID to set the panning rule on. This can be obtained from \ref AK_SoundEngine_GetOutputID. Set to 0 for primary output.
};

/// Starts or stops recording the sound engine audio output.
/// 
/// The capture system outputs a wav file per current output device of the sound engine.
/// If more than one device is active, the system will create multiple files in the same output 
/// directory and will append numbers at the end of the provided filename.
///
/// If no device is running yet, the command will return success AK_Success despite doing nothing.
/// Use AK::SoundEngine::RegisterAudioDeviceStatusCallback to get notified when devices are created/destructed.
///
/// \remark The sound engine opens a stream for writing using <tt>AK::IAkStreamMgr::CreateStd()</tt>. 
/// If you are using the default implementation of the Stream Manager, file opening is executed in your 
/// implementation of the Low-Level IO interface <tt>AK::StreamMgr::IAkLowLevelIOHook::BatchOpen()</tt>. 
/// The following AkFileSystemFlags are passed: uCompanyID = AKCOMPANYID_AUDIOKINETIC and uCodecID = AKCODECID_PCM,
///	and the AkOpenMode is AK_OpenModeWriteOvrwr. See \ref streamingmanager_lowlevel_location for
///	more details on managing the deployment of your Wwise generated data.
/// 
/// When \c isEnabled is set, the Sound Engine expects a string to follow the command representing the name of the file to write. For example:
/// 
///     auto cmd = (AkCmd_ControlOutputCapture*)AK_CommandBuffer_Add(buffer, AkCommand_ControlOutputCapture);
///     cmd->isEnabled = 1;
///     AK_CommandBuffer_AddString(buffer, "myrecording.wav");
/// 
/// This command can fail for the following reasons:
/// - \c AK_InvalidParameter if no valid file name follows the command when starting output capture.
/// \sa 
/// - AK::SoundEngine::StartOutputCapture
/// - AK::SoundEngine::StopOutputCapture
/// - AK::StreamMgr::SetFileLocationResolver
/// - \ref streamingdevicemanager
/// - \ref streamingmanager_lowlevel_location
/// - AK::SoundEngine::RegisterAudioDeviceStatusCallback
struct AkCmd_ControlOutputCapture
{
	AkUInt32 isEnabled;   ///< Whether the Sound Engine should capture the audio output to a file.
};

/// Adds text or binary marker in captured audio output file.
/// 
/// The Sound Engine expects either a string or binary data to follow this command. For example, to add a text marker:
/// 
///     auto cmd = (AkCmd_AddOutputCaptureMarker*)AK_CommandBuffer_Add(buffer, AkCommand_AddOutputCaptureMarker);
///     cmd->samplePos = AK_INVALID_SAMPLE_POS;
///     cmd->markerDataSize = strlen("my marker") + 1;
///     AK_CommandBuffer_AddString(buffer, "my marker");
/// 
/// To add a binary data marker:
/// 
///     auto cmd = (AkCmd_AddOutputCaptureMarker*)AK_CommandBuffer_Add(buffer, AkCommand_AddOutputCaptureMarker);
///     cmd->samplePos = AK_INVALID_SAMPLE_POS;
///     cmd->markerDataSize = sizeof(data);
///     AK_CommandBuffer_AddArray(buffer, sizeof(data), 1, data);
/// 
/// This command can fail for the following reasons:
/// - \c AK_InvalidParameter if \c markerDataSize is 0 or there is no marker data following the command
/// 
/// \sa
/// - AkCommand_AddOutputCaptureMarker
/// - AK::SoundEngine::AddOutputCaptureMarker
struct AkCmd_AddOutputCaptureMarker
{
	AkUInt32 markerDataSize;  ///< Size (in bytes) of the data following the command. For text markers, must include the null byte.
	AkUInt32 samplePos;       ///< Sample position to attach the marker to. If set to AK_INVALID_SAMPLE_POS, marker is added at the current recording time.
};

/// Enable/disable offline rendering, and control the size of each audio frame when offline rendering is enabled.
/// 
/// Offline rendering disconnects the Sound Engine from hardware audio output. Instead, the Sound Engine renders
/// one frame of audio every time AK::SoundEngine::RenderAudio is called. This audio frame is not audible, but 
/// it can be captured to a file using Output Capture (see <tt>AkCmd_ControlOutputCapture</tt>).
/// 
/// \sa
/// - AkCommand_ControlOfflineRendering
/// - AK::SoundEngine::SetOfflineRendering
/// - AK::SoundEngine::SetOfflineRenderingFrameTime
/// - AkCmd_ControlOutputCapture
/// - \ref goingfurther_offlinerendering
struct AkCmd_ControlOfflineRendering
{
	AkUInt32 isEnabled;             ///< Whether offline rendering should be activated or not.
	AkReal32 frameTimeInSeconds;    ///< Frame time in seconds used during offline rendering. Set to a negative value to leave the frame time the same as the last time it was set.
};

/// Sets the random seed value. Can be used to synchronize randomness
/// across instances of the Sound Engine.
/// 
/// \remark This seeds the number generator used for all container randomization 
///         and the plug-in RNG; since it acts globally, this should be called right 
///         before any PostEvent call where randomness synchronization is required,
///         and cannot guarantee similar results for continuous containers.
/// 
/// \sa
/// - AkCommand_SetRandomSeed
/// - AK::SoundEngine::SetRandomSeed
/// - AK::IAkPluginServiceRNG
struct AkCmd_SetRandomSeed
{
	AkUInt32 seedValue; ///< Seed value to use for random number generation
};

/// Allows streaming the first part of all streamed files referenced by an Event into a cache buffer. 
/// 
/// Caching streams are serviced when no other streams require the available bandwidth. 
/// The files will remain cached until a command disables caching, or a higher priority 
/// pinned file needs the space and the limit set by \c uMaxCachePinnedBytes is exceeded.  
/// 
/// \remarks The amount of data from the start of the file that will be pinned to cache is determined by the prefetch size. The prefetch size is set via the authoring tool and stored in the sound banks.  
/// \remarks It is possible to override the prefetch size stored in the sound bank via the low level IO. For more information see <tt>AK::StreamMgr::IAkLowLevelIOHook::BatchOpen()</tt> and \c AkFileSystemFlags.
/// \remarks If this function is called additional times with the same event, then the priority of the caching streams are updated. Note however that priority is passed down to the stream manager 
///	on a file-by-file basis, and if another event is pinned to cache that references the same file but with a different priority, then the first priority will be updated with the most recent value.
/// \remarks If the event references files that are chosen based on a State Group (via a switch container), all files in all states will be cached. Those in the current active state
/// will get cached with active priority, while all other files will get cached with inactive priority.
/// \remarks \c inactivePriority is only relevant for events that reference switch containers that are assigned to State Groups. This parameter is ignored for all other events, including events that only reference
/// switch containers that are assigned to Switch Groups. Files that are chosen based on a Switch Group have a different switch value per game object, and are all effectively considered active by the pin-to-cache system.
/// 
/// This command can fail for the following reasons:
/// - \c AK_InvalidParameter if \c eventID is invalid
/// - \c AK_IDNotFound if \c eventID refers to an unknown event
/// 
/// \sa
/// - AkCommand_ControlEventStreamCache
/// - AK::SoundEngine::PinEventInStreamCache
/// - AK::SoundEngine::UnpinEventInStreamCache
/// - AK::SoundEngine::GetBufferStatusForPinnedEvent
/// - AK::StreamMgr::IAkLowLevelIOHook::BatchOpen
/// - AkFileSystemFlags
struct AkCmd_ControlEventStreamCache
{
	AkUniqueID eventID;           ///< ID of event. Stream caching state will be updated for all streaming files referenced by this event.
	AkUInt8    isCached;          ///< Activate or de-activate stream caching. When false, any cache buffers previously associated with the event are released.
	AkPriority activePriority;    ///< Priority of active stream caching I/O
	AkPriority inactivePriority;  ///< Priority of inactive stream caching I/O
};

/// Puts the sound engine in background mode, where audio isn't processed anymore. Required when the platform has a background mode or some suspended state.
/// 
/// When suspended, the sound engine will process API messages (like PostEvent and SetSwitch) only when \ref AK::SoundEngine::RenderAudio is called. 
/// It is recommended to match the \c renderWhileSuspended parameter with the behavior of the rest of your game: 
/// 
/// - If your game still runs in background and you must keep some kind of coherent state between the audio engine and game, then allow rendering.
/// - If you want to minimize CPU when in background, then don't allow rendering and never call RenderAudio from the game.
/// 
/// Use the same command with \c isSuspended set to false when your application receives the message from the OS that the process is back in foreground.
///
/// Consult \ref workingwithsdks_system_calls to learn when it is appropriate to call this function for each platform.
/// 
/// \sa
/// - AkCommand_ControlSuspendedState
/// - AK::SoundEngine::Suspend
/// - AK::SoundEngine::WakeupFromSuspend
/// - \ref workingwithsdks_system_calls
struct AkCmd_ControlSuspendedState
{
	AkUInt8  isSuspended;           ///< Whether to suspend or wake up the Sound Engine.
	AkUInt8  renderWhileSuspended;  ///< ///< If set to true, audio processing will still occur while suspended, but not outputted. When set to false, no audio will be processed at all, even when AK::SoundEngine::RenderAudio is called.
	AkTimeMs transitionTime;        ///< Delay the transition. During transition to suspended state, a fade-out is applied to the audio output. When 0, the suspend takes effect immediately but audio may glitch.
};

/// Mutes/Unmutes the busses tagged as background music.
/// 
/// This is automatically called for platforms that have user-music support.
/// This command is provided to give the same behavior on platforms that don't have user-music support.
/// 
/// \sa
/// - AkCommand_MuteBackgroundMusic
/// - AK::SoundEngine::MuteBackgroundMusic
struct AkCmd_MuteBackgroundMusic
{
	AkUInt32 isMuted;  ///< Whether to mute BGM busses.
};

/// Sends custom game data to a plug-in that resides on a bus (effect plug-in) or a voice (source plug-in).
/// 
/// When \c dataSize is greater than 0, the Sound Engine expects binary data to follow this command. For example:
/// 
///     auto cmd = (AkCmd_SendPluginCustomGameData*)AK_CommandBuffer_Add(buffer, AkCmd_SendPluginCustomGameData);
///     cmd->busID = myBusID;
///     cmd->pluginType = AkPluginTypeEffect;
///     cmd->companyID = myCompanyID;
///     cmd->pluginID = myPluginID;
///     cmd->dataSize = plugin_data.size();
///     AK_CommandBuffer_AddArray(buffer, 1, plugin_data.size(), plugin_data.data());
/// 
/// Data will be copied and stored into a separate list.
/// Previous entry is deleted when a new one is sent.
/// Set \c dataSize to 0 to clear item from the list.
/// 
/// \aknote The plug-in type and ID are passed and matched with plugins set on the desired bus. 
/// This means that multiple instances of the same plug-in on a given bus' effect chain will always receive the same data.
/// \endaknote
/// 
/// This command can fail for the following reasons:
/// - \c AK_InvalidParameter: \c pluginID is invalid, or there is less binary data following the command than what \c dataSize declares.
/// - \c AK_InsufficientMemory: Not enough memory to complete the operation.
/// 
/// \sa
/// - AkCommand_SendPluginCustomGameData
/// - AK::SoundEngine::SendPluginCustomGameData
struct AkCmd_SendPluginCustomGameData
{
	AkUniqueID     busID;           ///< Bus ID. For source plug-ins, specify AK_INVALID_UNIQUE_ID.
	AkGameObjectID gameObjectID;    ///< Game Object ID. Pass AK_INVALID_GAME_OBJECT to send custom data with global scope. Game object scope supersedes global scope, as with RTPCs. 
	AkUInt32       pluginType;      ///< Plug-in type (for example, source or effect). See \ref AkPluginType.
	AkUInt32       companyID;       ///< Company identifier (as declared in the plug-in description XML file)
	AkUInt32       pluginID;        ///< Plug-in identifier (as declared in the plug-in description XML file)
	AkUInt32       dataSize;        ///< Size of data in bytes
};

/// Assign a game object as the Spatial Audio listener.  There can be only one Spatial Audio listener registered at any given time; <tt>gameObjectID</tt> will replace any previously set Spatial Audio listener.
/// The game object passed in must be registered by the client, at some point, for sound to be heard.  It is not necessary to be registered at the time of calling this function.
/// If no listener is explicitly registered to spatial audio, then a default listener (set via \c AK::SoundEngine::SetDefaultListeners()) is selected.  If there are no default listeners, or there are more than one
/// default listeners, then it is necessary to call RegisterListener() to specify which listener to use with Spatial Audio.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID is not valid.
/// 
/// \sa
/// - AkCommand_SA_RegisterListener
/// - AK::SpatialAudio::RegisterListener
struct AkCmd_SA_RegisterListener
{
	AkGameObjectID gameObjectID;
	bool primaryListener;
};

/// Unregister a game object as a listener in the SpatialAudio API; clean up Spatial Audio listener data associated with <tt>gameObjectID</tt>.  
/// If <tt>gameObjectID</tt> is the current registered listener, calling this function will clear the Spatial Audio listener and
/// Spatial Audio features will be disabled until another listener is registered.
/// This function is optional - listener are automatically unregistered when their game object is deleted in the sound engine.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID is not valid.
/// 
/// \sa 
/// - AkCommand_SA_UnregisterListener
/// - \ref AK::SpatialAudio::UnregisterListener
struct AkCmd_SA_UnregisterListener
{
	AkGameObjectID gameObjectID;
};

/// Add or update an individual image source for processing via the AkReflect plug-in.  Use this API for detailed placement of
/// reflection image sources, whose positions have been determined by the client, such as from the results of a ray cast, computation or by manual placement.  One possible
/// use case is generating reflections that originate far enough away that they can be modeled as a static point source, for example, off of a distant mountain.
/// The SpatialAudio API manages image sources added via AkCmd_SA_SetImageSource and sends them to the AkReflect plug-in that is on the aux bus with ID \c auxBusID. 
/// The image source applies all game objects that have a reflections aux send defined in the authoring tool, or only to a specific game object if \c gameObjectID is used.
/// \aknote The \c AkImageSourceSettings struct passed in \c info must contain a unique image source ID to be able to identify this image source across frames and when updating and/or removing it later.  
/// Each instance of AkReflect has its own set of data, so you may reuse ID, if desired, as long as \c in_gameObjectID and \c in_AuxBusID are different.
/// \aknote It is possible for the AkReflect plugin to process reflections from both \c SetImageSource and the geometric reflections API on the same aux bus and game object, but be aware that image source ID collisions are possible.
/// The image source IDs used by the geometric reflections API are generated from hashed data that uniquely identifies the reflecting surfaces. If a collision occurs, one of the reflections will not be heard.
/// While collision are rare, to ensure that it never occurs use an aux bus for \c SetImageSource that is unique from the aux bus(es) defined in the authoring tool, and from those passed to \c SetEarlyReflectionsAuxSend.
/// \endaknote
/// \aknote For proper operation with AkReflect and the SpatialAudio API, any aux bus using AkReflect should have 'Listener Relative Routing' checked and the 3D Spatialization set to None in the Wwise authoring tool. See \ref spatial_audio_wwiseprojectsetup_businstances for more details. \endaknote
/// 
/// Optionally, you can associate a name to the image source for profiling purposes. Call AK_CommandBuffer_AddString after adding the command to attach a name to the image source:
/// 
///     auto cmd = (AkCmd_SA_SetImageSource*)AK_CommandBuffer_Add(buffer, AkCommand_SA_SetImageSource);
///     // Fill command...
///     AK_CommandBuffer_AddString(buffer, "My image source");
/// 
/// This command can fail for the following reasons:
/// - AK_IDNotFound: \c gameObjectID was specified but is not registered.
/// 
/// \sa 
/// - AkCommand_SA_SetImageSource
/// - AK_CommandBuffer_AddString
///	- \ref AkCmd_SA_RemoveImageSource
///	- \ref AkCmd_SA_ClearImageSources
/// - \ref spatial_audio_wwiseprojectsetup_businstances
struct AkCmd_SA_SetImageSource
{
	AkImageSourceID      imageSourceID; ///< The ID of the image source being added.
	struct AkImageSourceSettings info;  ///< Image source information.
	AkUniqueID           auxBusID;      ///< Aux bus that has the AkReflect plug in for early reflection DSP. Pass AK_INVALID_AUX_ID to use the reflections aux bus defined in the authoring tool.
	AkGameObjectID       gameObjectID;  ///< The ID of the emitter game object to which the image source applies. Pass AK_INVALID_GAME_OBJECT to apply to all game objects that have a reflections aux bus assigned in the authoring tool.
};

/// Remove an individual reflection image source that was previously added via \c SetImageSource.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c imageSourceID is invalid
/// - AK_IDNotFound: \c gameObjectID was specified but is not registered.
/// 
/// \sa 
/// - AkCommand_SA_RemoveImageSource
///	- \ref AkCmd_SA_SetImageSource
///	- \ref AkCmd_SA_ClearImageSources
struct AkCmd_SA_RemoveImageSource
{
	AkUniqueID     imageSourceID; ///< The ID of the image source to remove.
	AkUniqueID     auxBusID;      ///< Aux bus that was passed to SetImageSource.
	AkGameObjectID gameObjectID;  ///< Game object ID that was passed to SetImageSource.
};

/// Remove all image sources matching \c auxBusID and \c gameObjectID that were previously added via \c SetImageSource.
/// Both \c auxBusID and \c gameObjectID can be treated as wild cards matching all aux buses and/or all game object, by passing \c AK_INVALID_AUX_ID and/or \c AK_INVALID_GAME_OBJECT, respectively.
/// 
/// This command can fail for the following reasons:
/// - AK_IDNotFound: \c gameObjectID was specified but is not registered.
/// 
/// \sa 
/// - AkCommand_SA_ClearImageSources
///	- \ref AkCmd_SA_SetImageSource
///	- \ref AkCmd_SA_RemoveImageSource
struct AkCmd_SA_ClearImageSources
{
	AkUniqueID     auxBusID;       ///< Aux bus that was passed to SetImageSource, or AK_INVALID_AUX_ID to match all aux busses.
	AkGameObjectID gameObjectID;   ///< Game object ID that was passed to SetImageSource, or AK_INVALID_GAME_OBJECT to match all game objects.
};

/// Add or update a set of geometry from the \c SpatialAudio module for geometric reflection and diffraction processing. A geometry set is a logical set of vertices, triangles, and acoustic surfaces,
/// which are referenced by the same \c AkGeometrySetID. The ID (\c geometrySetID) must be unique and is also chosen by the client in a manner similar to \c AkGameObjectID's.
/// It is necessary to create at least one geometry instance for each geometry set that is to be used for diffraction and reflection simulation.
/// 
/// The Sound Engine expects geometry data to be added right after the command. Call AK_CommandBuffer_AddGeometry to add geometry data:
/// 
///     AkGeometryParams geoParams; // Initialize with valid triangles, vertices, surfaces...
///     auto cmd = (AkCmd_SA_SetGeometry*)AK_CommandBuffer_Add(buffer, AkCommand_SA_SetGeometry);
///     cmd->geometrySetID = myID;
///     AK_CommandBuffer_AddGeometry(buffer, &geoParams);
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c geometrySetID is invalid, or missing geometry data after the command
/// - AK_InsufficientMemory: Not enough memory to complete the operation
/// 
/// \sa 
///	- \ref AkGeometryParams
/// - \ref AkCommand_SA_SetGeometry
///	- \ref AkCmd_SA_SetGeometryInstance
///	- \ref AkCmd_SA_RemoveGeometry
struct AkCmd_SA_SetGeometry
{
	AkGeometrySetID         geometrySetID; ///< Unique geometry set ID, chosen by client.
};

/// Remove a set of geometry to the SpatialAudio API.
/// Calling \c AK::SpatialAudio::RemoveGeometry will remove all instances of the geometry from the scene.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c geometrySetID is invalid
/// - AK_IDNotFound: The ID passed does not correspond to a registered geometry set ID.
/// 
/// \sa 
/// - \ref AkCommand_SA_RemoveGeometry
///	- \ref AkCmd_SA_SetGeometry
struct AkCmd_SA_RemoveGeometry
{
	AkGeometrySetID geometrySetID; ///< ID of geometry set to be removed.
};

/// Add or update a geometry instance from the \c SpatialAudio module for geometric reflection and diffraction processing. 
/// A geometry instance is a unique instance of a geometry set with a specified transform (position, rotation and scale). 
/// It is necessary to create at least one geometry instance for each geometry set that is to be used for diffraction and reflection simulation.
/// The ID (\c geometryInstanceID) must be unique amongst all geometry instances, including geometry instances referencing different geometry sets. The ID is chosen by the client in a manner similar to \c AkGameObjectID's.
/// To update the transform of an existing geometry instance, call SetGeometryInstance again, passing the same \c AkGeometryInstanceID, with the updated transform. 
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c geometryInstanceID or \c params.GeometrySetID are not valid IDs or \c params.PositionAndOrientation is not a valid transform.
/// - AK_IDNotFound: The \c GeometrySetID does not refer to a registered geometry set.
/// - AK_InsufficientMemory: Not enough memory to complete the operation.
/// 
/// \sa 
/// - \ref AkCommand_SA_SetGeometryInstance
///	- \ref AkGeometryInstanceParams
///	- \ref AkCmd_SA_RemoveGeometryInstance
struct AkCmd_SA_SetGeometryInstance
{
	AkGeometryInstanceID            geometryInstanceID; ///< Unique geometry set instance ID, chosen by client.
	struct AkGeometryInstanceParams params;             ///< Geometry instance parameters to set.
};

/// Remove a geometry instance from the SpatialAudio API.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c geometryInstanceID is not valid.
/// - AK_IDNotFound: \c geometryInstanceID does not refer to a registered geometry instance.
/// 
/// \sa 
/// - \ref AkCommand_SA_RemoveGeometryInstance
///	- \ref AkCmd_SA_SetGeometryInstance
struct AkCmd_SA_RemoveGeometryInstance
{
	AkGeometryInstanceID geometryInstanceID; ///< ID of geometry set instance to be removed.
};

/// Add or update a room. Rooms are used to connect portals and define an orientation for oriented reverbs. This function may be called multiple times with the same ID to update the parameters of the room.
/// \akwarning The ID (\c roomID) must be chosen in the same manner as \c AkGameObjectID's, as they are in the same ID-space. The spatial audio lib manages the 
/// registration/unregistration of internal game objects for rooms that use these IDs and, therefore, must not collide. 
/// Also, the room ID must not be in the reserved range (AkUInt64)(-32) to (AkUInt64)(-2) inclusively. You may, however, explicitly add the default room ID <tt>AK_OUTDOORS_ROOM_ID</tt> (-1) 
/// in order to customize its AkRoomParams, to provide a valid auxiliary bus, for example.\endakwarning
/// 
/// Optionally, you can associate a name to the room for profiling purposes. Call AK_CommandBuffer_AddString after adding the command to attach a name to the room:
/// 
///     auto cmd = (AkCmd_SA_SetRoom*)AK_CommandBuffer_Add(buffer, AkCommand_SA_SetRoom);
///     // Fill command...
///     AK_CommandBuffer_AddString(buffer, "Bedroom 1");
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c roomID is outside the valid range
/// - AK_InsufficientMemory: Not enough memory to complete the operation.
/// 
/// \sa
/// - \ref AkCommand_SA_SetRoom
/// - \ref AK_CommandBuffer_AddString
/// - \ref ::AkRoomID
/// - \ref AkRoomParams
/// - \ref AkCmd_SA_RemoveRoom
struct AkCmd_SA_SetRoom
{
	AkRoomID             roomID;    ///< Unique room ID, chosen by the client.
	struct AkRoomParams  params;    ///< Parameter for the room.
};

/// Remove a room.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c roomID is outside the valid range
/// - AK_IDNotFound: Provided \c roomID is not recognized as a registered portal ID.
/// 
/// \sa
/// - \ref AkCommand_SA_RemoveRoom
/// - \ref ::AkRoomID
/// - \ref AkCmd_SA_SetRoom
struct AkCmd_SA_RemoveRoom
{
	AkRoomID roomID; ///< Room ID that was specified by \ref AkCmd_SA_SetRoom.
};

/// Add or update an acoustic portal. A portal is an opening that connects two or more rooms to simulate the transmission of reverberated (indirect) sound between the rooms. 
/// This function may be called multiple times with the same ID to update the parameters of the portal. The ID (\c portalID) must be chosen in the same manner as \c AkGameObjectID's, 
/// as they are in the same ID-space. The spatial audio lib manages the registration/unregistration of internal game objects for portals that use these IDs, and therefore must not collide.
/// 
/// Optionally, you can associate a name to the portal for profiling purposes. Call AK_CommandBuffer_AddString after adding the command to attach a name to the portal:
/// 
///     auto cmd = (AkCmd_SA_SetPortal*)AK_CommandBuffer_Add(buffer, AkCommand_SA_SetPortal);
///     // Fill command...
///     AK_CommandBuffer_AddString(buffer, "Portal 1");
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c portalID or some portal parameters are not valid.
/// - AK_InsufficientMemory: Not enough memory to complete the operation.
/// 
/// \sa
/// - \ref AkCommand_SA_SetPortal
/// - \ref AkPortalID
/// - \ref AkPortalParams
/// - \ref AkCmd_SA_RemovePortal
struct AkCmd_SA_SetPortal
{
	AkPortalID            portalID;    ///< Unique portal ID, chosen by the client.
	struct AkPortalParams params;      ///< Parameter for the portal.
};

/// Set the obstruction and occlusion value for a portal that has been registered with Spatial Audio.
/// Portal obstruction simulates objects that block the direct sound path between the portal and the listener, but
/// allows indirect sound to pass around the obstacle. For example, use portal obstruction 
/// when a piece of furniture blocks the line of sight of the portal opening.
/// Portal obstruction is applied to the connection between the emitter and the listener, and only affects the dry signal path.
/// Portal occlusion simulates a complete blockage of both the direct and indirect sound through a portal. For example, use portal occlusion for 
/// opening or closing a door or window.
/// Portal occlusion is applied to the connection between the emitter and the first room in the chain, as well as the connection between the emitter and listener.
/// Portal occlusion affects both the dry and wet (reverberant) signal paths.
/// If you are using built-in game parameters to drive RTPCs, the obstruction and occlusion values set here 
/// do not affect the RTPC values. This behavior is intentional and occurs because RTPCs only provide one 
/// value per game object, but a single game object can have multiple paths through different Portals, 
/// each with different obstruction and occlusion values.
/// To apply detailed obstruction to specific sound paths but not others, use \c AkCmd_SA_SetGameObjectToPortalObstruction and \c AkCmd_SA_SetPortalToPortalObstruction.
/// To apply occlusion and obstruction to the direct line of sight between the emitter and listener use \c AkCmd_SetObjectObstructionAndOcclusion.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c portalID is not valid, or \c obstruction / \c occlusion values are out of range.
/// - AK_IDNotFound: \c portalID is not a registered portal.
/// 
/// \sa
/// - \ref AkCommand_SA_SetPortalObstructionAndOcclusion
/// - \ref AkCmd_SA_SetGameObjectToPortalObstruction
/// - \ref AkCmd_SA_SetPortalToPortalObstruction
/// - \ref AkCmd_SetObjectObstructionAndOcclusion
struct AkCmd_SA_SetPortalObstructionAndOcclusion
{
	AkPortalID portalID;    ///< Portal ID.
	AkReal32   obstruction; ///< Obstruction value.  Valid range 0.f-1.f
	AkReal32   occlusion;   ///< Occlusion value.  Valid range 0.f-1.f
	bool       transition;  ///< Transition obstruction and occlusion through portals.  Default false
};

/// Set the obstruction value of the path between a game object and a portal that has been created by Spatial Audio.
/// The obstruction value is applied on one of the path segments of the sound between the emitter and the listener.
/// Diffraction must be enabled on the sound for a diffraction path to be created.
/// Also, there should not be any portals between the provided game object and portal ID parameters.
/// The obstruction value is used to simulate objects between the portal and the game object that are obstructing the sound.
/// Send an obstruction value of 0 to ensure the value is removed from the internal data structure.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID or \c portalID are not valid IDs, or \c obstruction is not within the accepted range.
/// - AK_IDNotFound: \c gameObjectID is not a registered game object or \c portalID is not a registered portal.
/// 
/// \sa
/// - \ref AkCommand_SA_SetGameObjectToPortalObstruction
/// - \ref AkCmd_SA_SetPortalObstructionAndOcclusion
struct AkCmd_SA_SetGameObjectToPortalObstruction
{
	AkGameObjectID gameObjectID; ///< Game object ID
	AkPortalID     portalID;     ///< Portal ID
	AkReal32       obstruction;  ///< Obstruction value.  Valid range 0.f-1.f
};

/// Set the obstruction value of the path between two portals that has been created by Spatial Audio.
/// The obstruction value is applied on one of the path segments of the sound between the emitter and the listener.
/// Diffraction must be enabled on the sound for a diffraction path to be created.
/// Also, there should not be any portals between the two provided ID parameters.
/// The obstruction value is used to simulate objects between the portals that are obstructing the sound.
/// Send an obstruction value of 0 to ensure the value is removed from the internal data structure.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c portalID0 or \c portalID1 are not valid IDs, or obstruction is not within the accepted range.
/// 
/// \sa
/// - \ref AkCommand_SA_SetPortalToPortalObstruction
/// - \ref AkCmd_SA_SetPortalObstructionAndOcclusion
struct AkCmd_SA_SetPortalToPortalObstruction
{
	AkPortalID portalID0;   ///< Portal ID
	AkPortalID portalID1;   ///< Portal ID
	AkReal32   obstruction; ///< Obstruction value.  Valid range 0.f-1.f
};

/// Remove a portal.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c portalID is not valid.
/// - AK_IDNotFound: Provided \c portalID is not recognized as a registered portal ID.
/// 
/// \sa
/// - \ref AkCommand_SA_RemovePortal
/// - \ref AkPortalID
/// - \ref AK::SpatialAudio::SetPortal
struct AkCmd_SA_RemovePortal
{
	AkPortalID portalID; ///< ID of portal to be removed, which was originally specified by \ref AkCmd_SA_SetPortal.
};

/// Use a Room as a Reverb Zone.
/// <tt>AkCmd_SA_SetReverbZone</tt> establishes a parent-child relationship between two Rooms and allows for sound propagation between them
/// as if they were the same Room, without the need for a connecting Portal. Setting a Room as a Reverb Zone 
/// is useful in situations where two or more acoustic environments are not easily modeled as closed rooms connected by portals.
/// Possible uses for Reverb Zones include: a covered area with no walls, a forested area within an outdoor space, or any situation 
/// where multiple reverb effects are desired within a common space. Reverb Zones have many advantages compared to standard Game-Defined
/// Auxiliary Sends. They are part of the Diffuse Propagation Path, and form reverb chains with other Rooms; they are spatialized according to their 3D extent;  
/// they are also subject to other acoustic phenomena simulated in Wwise Spatial Audio, such as diffraction and transmission.
/// A parent Room may have multiple Reverb Zones, but a Reverb Zone can only have a single Parent. If a Room is already assigned 
/// to a parent Room, it will first be removed from the old parent (exactly as if \ref AkCmd_SA_RemoveReverbZone was used) 
/// before then being assigned to the new parent Room. A Room can not be its own parent.
/// The Reverb Zone and its parent are both Rooms, and as such, must be specified using \ref AkCmd_SA_SetRoom.
/// If AkCmd_SA_SetReverbZone is sent before \ref AkCmd_SA_SetRoom, and either of the two rooms do not yet exist,
/// placeholder Rooms with default parameters are created. They should be subsequently parametrized with \ref AkCmd_SA_SetRoom.
/// 
/// To set which Reverb Zone a Game Object is in, use the \ref AkCmd_SA_SetGameObjectInRoom API, and pass the Reverb Zone's Room ID. 
/// In Wwise Spatial Audio, a Game Object can only ever be inside a single room, and Reverb Zones are no different in this regard.
/// \aknote
/// The automatically created 'outdoors' Room is commonly used as a parent Room for Reverb Zones, since they often model open spaces. 
/// To attach a Reverb zone to outdoors, set <tt>AK_OUTDOORS_ROOM_ID</tt> as the \c parentRoomID argument. Like all Rooms, the 'outdoors' Room
/// can be parameterized (for example, to assign a reverb bus) by specifying <tt>AK_OUTDOORS_ROOM_ID</tt> in a \ref AkCmd_SA_SetRoom command./// This command can fail for the following reasons:
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c reverbZoneRoomID or \c parentRoomID are not valid room IDs, or they are the same value.
/// - AK_InsufficientMemory: Not enough memory to complete the operation.
/// 
/// \sa
/// - \ref AkCommand_SA_SetReverbZone
/// - \ref AkRoomID
///	- \ref AkCmd_SA_SetRoom
///	- \ref AkCmd_SA_RemoveRoom
///	- \ref AkCmd_SA_RemoveReverbZone
/// - \ref AK_OUTDOORS_ROOM_ID
struct AkCmd_SA_SetReverbZone
{
	AkRoomID reverbZoneRoomID;      ///< ID of the Room which will be specified as a Reverb Zone. 
	AkRoomID parentRoomID;          ///< ID of the parent Room.
	AkReal32 transitionRegionWidth; ///< Width of the transition region between the Reverb Zone and its parent. The transition region is centered around the Reverb Zone geometry. It only applies where triangle transmission loss is set to 0.
};

/// Remove a Reverb Zone from its parent. 
/// It will no longer be possible for sound to propagate between the two rooms, unless they are explicitly connected with a Portal.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c reverbZoneRoomID is not a valid room ID
/// - AK_IDNotFound: Provided \c reverbZoneRoomID is not recognized as a registered room ID.
/// 
/// \sa
/// - \ref AkCommand_SA_RemoveReverbZone
///	- \ref AkCmd_SA_SetReverbZone
///	- \ref AkCmd_SA_RemoveRoom
///	- \ref AkCmd_SA_RemoveReverbZone
struct AkCmd_SA_RemoveReverbZone
{
	AkRoomID reverbZoneRoomID; ///< ID of the Room which has been specified as a Reverb Zone. 
};

/// Set the room that the game object is currently located in - usually the result of a containment test performed by the client. The room must have been registered with \ref AkCmd_SA_SetRoom.
///	Setting the room for a game object provides the basis for the sound propagation service, and also sets which room's reverb aux bus to send to.  The sound propagation service traces the path
/// of the sound from the emitter to the listener, and calculates the diffraction as the sound passes through each portal.  The portals are used to define the spatial location of the diffracted and reverberated
/// audio.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID or \c roomID are not valid IDs.
/// - AK_IDNotFound: Provided \c gameObjectID is not recognized as a registered game object.
/// - AK_InsufficientMemory: Not enough memory to complete the operation.
/// 
/// \sa
/// - \ref AkCommand_SA_SetGameObjectInRoom
///	- \ref AkCmd_SA_SetRoom
///	- \ref AkCmd_SA_RemoveRoom
struct AkCmd_SA_SetGameObjectInRoom
{
	AkGameObjectID gameObjectID; ///< Game object ID 
	AkRoomID       roomID;       ///< RoomID that was passed to \ref AkCmd_SA_SetRoom
};

/// Unset the room that the game object is currently located in.
///	When a game object has not been explicitly assigned to a room with \ref AkCmd_SA_SetGameObjectInRoom, the room is automatically computed.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID is not valid.
/// - AK_IDNotFound: Provided \c gameObjectID is not recognized as a registered game object.
/// - AK_InsufficientMemory: Not enough memory to complete the operation.
/// 
/// \sa
/// - \ref AkCommand_SA_UnsetGameObjectInRoom
///	- \ref AkCmd_SA_SetRoom
///	- \ref AkCmd_SA_RemoveRoom
struct AkCmd_SA_UnsetGameObjectInRoom
{
	AkGameObjectID gameObjectID; ///< Game object ID
};

/// Define an inner and outer radius around each sound position for a specified game object.
/// If the radii are set to 0, the game object is a point source. Non-zero radii create a Radial Emitter.
/// The radii are used in spread and distance calculations that simulates sound emitting from a spherical volume of space.
/// When applying attenuation curves, the distance between the listener and the inner sphere (defined by the sound position and \c innerRadius) is used. 
/// The spread for each sound position is calculated as follows:
/// - If the listener is outside the outer radius, the spread is defined by the area that the sphere occupies in the listener field of view. Specifically, this angle is calculated as <tt>2.0*asinf( outerRadius / distance )</tt>, where distance is the distance between the listener and the sound position.
///	- When the listener intersects the outer radius (the listener is exactly \c outerRadius units away from the sound position), the spread is exactly 50%.
/// - When the listener is between the inner and outer radii, the spread interpolates linearly from 50% to 100% as the listener transitions from the outer radius towards the inner radius.
/// - If the listener is inside the inner radius, the spread is 100%.
/// \aknote Transmission and diffraction calculations in Spatial Audio always use the center of the sphere (the position(s) passed into \c AkCmd_SetPosition or \c AkCmd_SetMultiplePositions) for raycasting. 
/// To obtain accurate diffraction and transmission calculations for radial sources, where different parts of the volume may take different paths through or around geometry,
/// it is necessary to pass multiple sound positions into \c AkCmd_SetMultiplePositions to allow the engine to 'sample' the area at different points.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID is invalid or \c outerRadius > \c innerRadius
/// - AK_IDNotFound: \c gameObjectID is specified but is not a registered game object.
/// - AK_InsufficientMemory: Not enough memory to complete the operation.
/// 
/// \sa
/// - \ref AkCommand_SA_SetGameObjectRadius
/// - \ref AkCmd_SetPosition
/// - \ref AkCmd_SetMultiplePositions
struct AkCmd_SA_SetGameObjectRadius
{
	AkGameObjectID gameObjectID;  ///< Game object ID
	AkReal32       outerRadius;   ///< Outer radius around each sound position, defining 50% spread. Must satisfy \c innerRadius <= \c outerRadius.
	AkReal32       innerRadius;   ///< Inner radius around each sound position, defining 100% spread and 0 attenuation distance. Must satisfy \c innerRadius <= \c outerRadius.
};

/// Set an early reflections auxiliary bus for a particular game object. 
/// Geometrical reflection calculation inside spatial audio is enabled for a game object if any sound playing on the game object has a valid early reflections aux bus specified in the authoring tool,
/// or if an aux bus is specified via \c AkCmd_SA_SetEarlyReflectionsAuxSend.
/// The \c auxBusID parameter of AkCmd_SA_SetEarlyReflectionsAuxSend applies to sounds playing on the game object \c gameObjectID which have not specified an early reflection bus in the authoring tool -
/// the parameter specified on individual sounds' reflection bus takes priority over the value passed in to \c AkCmd_SA_SetEarlyReflectionsAuxSend.
/// \aknote 
/// Users may apply this function to avoid duplicating sounds in the Containers hierarchy solely for the sake of specifying a unique early reflection bus, or in any situation where the same 
/// sound should be played on different game objects with different early reflection aux buses (the early reflection bus must be left blank in the authoring tool if the user intends to specify it through the API). \endaknote
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID is invalid.
/// - AK_IDNotFound: \c gameObjectID is not a registered game object.
/// 
/// \sa
/// - \ref AkCommand_SA_SetEarlyReflectionsAuxSend
struct AkCmd_SA_SetEarlyReflectionsAuxSend
{
	AkGameObjectID gameObjectID; ///< Game object ID 
	AkAuxBusID     auxBusID;     ///< Auxiliary bus ID. Applies only to sounds which have not specified an early reflection bus in the authoring tool. Set \c AK_INVALID_AUX_ID to set only the send volume.
};

/// Set an early reflections send volume for a particular game object. 
/// The \c volume parameter is used to control the volume of the early reflections send. It is combined with the early reflections volume specified in the authoring tool, and is applied to all sounds 
/// playing on the game object.
/// Setting \c volume to 0.f will disable all reflection processing for this game object.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID is invalid.
/// - AK_IDNotFound: \c gameObjectID is specified but is not a registered game object. 
/// 
/// \sa
/// - \ref AkCommand_SA_SetEarlyReflectionsVolume
struct AkCmd_SA_SetEarlyReflectionsVolume
{
	AkGameObjectID gameObjectID; ///< Game object ID 
	AkReal32       volume;       ///< Send volume (linear) for auxiliary send. Set 0.f to disable reflection processing. Valid range 0.f-1.f. 
};

/// Set a global scaling factor that manipulates reverb send values. AK::SpatialAudio::SetAdjacentRoomBleed affects the proportion of audio sent to adjacent rooms 
/// and to the emitter's current room. It updates the initialization setting specified in AkSpatialAudioInitSettings::fAdjacentRoomBleed.
/// This value is multiplied by AkPortalParams::AdjacentRoomBleed, which is used to scale reverb bleed for individual portals.
/// When calculating reverb send amounts, each portal's aperture is multiplied by fAdjacentRoomBleed, which alters its perceived size:
///	- 1.0 (default): Maintains portals at their true geometric sizes (no scaling).
///	- > 1.0: Increases the perceived size of all portals, which increases bleed into adjacent rooms.
///	- < 1.0: Decreases the perceived size of all portals, which reduces bleed into adjacent rooms.
/// Valid range: (0.0 - infinity)
/// Note: Values close to 0 might cause abrupt portal transitions.
struct AkCmd_SA_SetAdjacentRoomBleed
{
	AkReal32 adjacentRoomBleed;
};

/// Set the early reflections order for reflection calculation. The reflections order indicates the number of times sound can bounce off of a surface. 
/// A higher number requires more CPU resources but results in denser early reflections. Set to 0 to globally disable reflections processing.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c reflectionsOrder is not in the accepted range.
/// 
/// \sa
/// - \ref AkCommand_SA_SetReflectionsOrder
struct AkCmd_SA_SetReflectionsOrder
{
	AkUInt32 reflectionsOrder; ///< Number of reflections to calculate. Valid range [0,4]
	bool     updatePaths;      ///< Set to true to clear existing higher-order paths and to force the re-computation of new paths. If false, existing paths will remain and new paths will be computed when the emitter or listener moves.
};

/// Set the diffraction order for geometric path calculation. The diffraction order indicates the number of edges a sound can diffract around. 
/// A higher number requires more CPU resources but results in paths found around more complex geometry. Set to 0 to globally disable geometric diffraction processing.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c diffractionOrder is not in the accepted range.
/// 
/// \sa
/// - \ref AkCommand_SA_SetDiffractionOrder
/// - \ref AkSpatialAudioInitSettings::uMaxDiffractionOrder
struct AkCmd_SA_SetDiffractionOrder
{
	AkUInt32 diffractionOrder;  ///< Number of diffraction edges to consider in path calculations. Valid range [0,8]
	bool     updatePaths;       ///< Set to true to clear existing diffraction paths and to force the re-computation of new paths. If false, existing paths will remain and new paths will be computed when the emitter or listener moves.
};

/// Set the maximum number of computed reflection paths.
/// 
/// \sa
/// - \ref AkCommand_SA_SetMaxGlobalReflectionPaths
struct AkCmd_SA_SetMaxGlobalReflectionPaths
{
	AkUInt32 maxReflectionPaths; ///< Maximum number of reflection paths. Valid range [0..[
};

/// Set the maximum number of game-defined auxiliary sends that can originate from a single emitter. 
/// Set to 1 to only allow emitters to send directly to their current room. Set to 0 to disable the limit.
/// \sa
/// - \ref AkCommand_SA_SetMaxEmitterRoomAuxSends
/// - \ref AkSpatialAudioInitSettings::uMaxEmitterRoomAuxSends
struct AkCmd_SA_SetMaxEmitterRoomAuxSends
{
	AkUInt32 maxEmitterRoomAuxSends;	///< The maximum number of room aux send connections.
};

/// Set the maximum number of computed diffraction paths. 
/// Pass a valid Game Object ID to to \c gameObjectID to affect a specific game object and override the value set in <tt>AkSpatialAudioInitSettings::uMaxDiffractionPaths</tt>. 
/// Pass \c AK_INVALID_GAME_OBJECT to apply the same limit to all Game Objects (that have not previously been passed to \c AkCmd_SA_SetMaxDiffractionPaths), 
/// updating the value set for <tt>AkSpatialAudioInitSettings::uMaxDiffractionPaths</tt>.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c gameObjectID is not valid or \c maxDiffractionPaths is not within the accepted range.
/// - AK_IDNotFound: \c gameObjectID is specified but is not a registered game object. 
/// 
/// \sa
/// - \ref AkCommand_SA_SetMaxDiffractionPaths
/// - \ref AkSpatialAudioInitSettings::uMaxDiffractionPaths
struct AkCmd_SA_SetMaxDiffractionPaths
{
	AkUInt32       maxDiffractionPaths; ///< Maximum number of reflection paths. Valid range [0..32]
	AkGameObjectID gameObjectID;        ///< Game Object ID. Set \c AK_INVALID_GAME_OBJECT to affect all Game Objects, effectivly updating <tt>AkSpatialAudioInitSettings::uMaxDiffractionPaths</tt>. Pass a valid Game Object ID to override the init setting for a specific Game Object.
};

/// [\ref spatial_audio_experimental "Experimental"]  Enable parameter smoothing on the diffraction paths output from the Acoustics Engine, either globally or for a specific game object. Set \c smoothingConstantMs to a value greater than 0 to define the time constant (in milliseconds) for parameter smoothing. 
/// 
/// This command can fail for the following reasons:
/// - AK_IDNotFound: \c gameObjectID is specified but is not a registered game object. 
/// 
/// \sa
/// - \ref AkCommand_SA_SetSmoothingConstant
/// - \ref AkSpatialAudioInitSettings::fSmoothingConstantMs
struct AkCmd_SA_SetSmoothingConstant
{
	AkReal32       smoothingConstantMs; ///< Smoothing constant (ms)
	AkGameObjectID gameObjectID;        ///< Game Object ID. Set \c AK_INVALID_GAME_OBJECT to set the global smoothing constant, affecting all Spatial Audio Emitters and Rooms.
};

/// Set the operation used to calculate transmission loss on a direct path between emitter and listener.
/// 
/// This command can fail for the following reasons:
/// - AK_InvalidParameter: \c operation is not within the accepted range. 
/// 
/// \sa
/// - \ref AkCommand_SA_SetTransmissionOperation
struct AkCmd_SA_SetTransmissionOperation
{
	AkUInt32 operation; ///< The operation to be used on all transmission paths. Possible values are listed in the \ref AkTransmissionOperation enum.
};

/// Set the number of primary rays cast from the listener by the stochastic ray casting engine.
/// 
/// \sa
/// - \ref AkCommand_SA_SetNumberOfPrimaryRays
struct AkCmd_SA_SetNumberOfPrimaryRays
{
	AkUInt32 uNbPrimaryRays; ///< Number of rays cast from the listener
};

/// Set the number of frames on which the path validation phase will be spread. Value between [1..[
/// High values delay the validation of paths. A value of 1 indicates no spread at all.
/// 
/// \sa
/// - \ref AkCommand_SA_SetLoadBalancingSpread
struct AkCmd_SA_SetLoadBalancingSpread
{
	AkUInt32 uNbFrames; ///< Number of spread frames
};

/// Reset the stochastic engine state by re-initializing the random seeds.
///
/// \sa
/// - \ref AkCommand_SA_ResetStochasticEngine
struct AkCmd_SA_ResetStochasticEngine
{
	AkUInt32 uUnused;
};

/// Describes the data written at the beginning of any initialized command buffer.
struct AkCommandBufferHeader
{
	AkUInt32                bufferSize;                 ///<Total size of command buffer in bytes
	AkUInt32                lastCommandOffset;          ///<Internal use. Should be zero-initialized.
	AK_WPTR(AkCommandCallbackFunc, completionCallback); ///<If not null, this callback is called when all commands in the buffer have been processed. 
	                                                    ///<At that point in time, the client can free the command buffer memory, or re-use it for something else.
	AK_WPTR(void*,          completionCallbackCookie);  ///<User cookie for done_callback
};

/// Describes the data written at the beginning of each command in the command buffer
struct AkCommandHeader
{
	AkUInt16     code;    ///<Unique ID of the command, as listed in \ref AkCommand enum
	AkUInt16     size;    ///<Size of the payload (without this header), in bytes
	AkUInt16     flags;   ///<Internal flags for the command
	AkUInt16     result;  ///<Result of processing the command, as listed in \ref AKRESULT enum. This is set by the Sound Engine after the command is processed. 
	                      ///<When set to a value other than AK_Success, the command was not processed, and the error code indicates the reason why. 
	                      ///<Clients can inspect this value after the command buffer's done_callback is called. Its value has no meaning before this callback is called.
};

#pragma pack(pop)

/// Structure used to iterate through a command buffer.
/// \sa
/// - AK_CommandBuffer_Begin
/// - AK_CommandBuffer_Next
struct AkCommandBufferIterator
{
	struct AkCommandHeader* header;   ///<Pointer to header of command. Use this to check the code of the command and the result code after processing.
	void* payload;                    ///<Pointer to payload of current command
	void* buffer;                     ///<Pointer to beginning of command buffer
};
