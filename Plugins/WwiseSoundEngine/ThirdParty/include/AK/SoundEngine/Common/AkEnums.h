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

#include <AK/SoundEngine/Common/AkConstants.h>

enum AKRESULT
{
	AK_NotImplemented              = 0,          ///< This feature is not implemented.
	AK_Success                     = 1,          ///< The operation was successful.
	AK_Fail                        = 2,          ///< The operation failed.
	AK_PartialSuccess              = 3,          ///< The operation succeeded partially.
	AK_NotCompatible               = 4,          ///< Incompatible formats
	AK_AlreadyConnected            = 5,          ///< The stream is already connected to another node.
	AK_InvalidFile                 = 7,          ///< The provided file is the wrong format or unexpected values causes the file to be invalid.
	AK_AudioFileHeaderTooLarge     = 8,          ///< The file header is too large.
	AK_MaxReached                  = 9,          ///< The maximum was reached.
	AK_InvalidID                   = 14,         ///< The ID is invalid.
	AK_IDNotFound                  = 15,         ///< The ID was not found.
	AK_InvalidInstanceID           = 16,         ///< The InstanceID is invalid.
	AK_NoMoreData                  = 17,         ///< No more data is available from the source.
	AK_InvalidStateGroup           = 20,         ///< The StateGroup is not a valid channel.
	AK_ChildAlreadyHasAParent      = 21,         ///< The child already has a parent.
	AK_InvalidLanguage             = 22,         ///< The language is invalid (applies to the Low-Level I/O).
	AK_CannotAddItselfAsAChild     = 23,         ///< It is not possible to add itself as its own child.
	AK_InvalidParameter            = 31,         ///< Something is not within bounds, check the documentation of the function returning this code.
	AK_ElementAlreadyInList        = 35,         ///< The item could not be added because it was already in the list.
	AK_PathNotFound                = 36,         ///< This path is not known.
	AK_PathNoVertices              = 37,         ///< Stuff in vertices before trying to start it
	AK_PathNotRunning              = 38,         ///< Only a running path can be paused.
	AK_PathNotPaused               = 39,         ///< Only a paused path can be resumed.
	AK_PathNodeAlreadyInList       = 40,         ///< This path is already there.
	AK_PathNodeNotInList           = 41,         ///< This path is not there.
	AK_DataNeeded                  = 43,         ///< The consumer needs more.
	AK_NoDataNeeded                = 44,         ///< The consumer does not need more.
	AK_DataReady                   = 45,         ///< The provider has available data.
	AK_NoDataReady                 = 46,         ///< The provider does not have available data.
	AK_InsufficientMemory          = 52,         ///< Memory error.
	AK_Cancelled                   = 53,         ///< The requested action was cancelled (not an error).
	AK_UnknownBankID               = 54,         ///< Trying to load a bank using an ID which is not defined.
	AK_BankReadError               = 56,         ///< Error while reading a bank.
	AK_InvalidSwitchType           = 57,         ///< Invalid switch type (used with the switch container)
	AK_FormatNotReady              = 63,         ///< Source format not known yet.
	AK_WrongBankVersion            = 64,         ///< The bank version is not compatible with the current bank reader.
	AK_FileNotFound                = 66,         ///< File not found.
	AK_DeviceNotReady              = 67,         ///< Specified ID doesn't match a valid hardware device: either the device doesn't exist or is disabled.
	AK_BankAlreadyLoaded           = 69,         ///< The bank load failed because the bank is already loaded.
	AK_RenderedFX                  = 71,         ///< The effect on the node is rendered.
	AK_ProcessNeeded               = 72,         ///< A routine needs to be executed on some CPU.
	AK_ProcessDone                 = 73,         ///< The executed routine has finished its execution.
	AK_MemManagerNotInitialized    = 74,         ///< The memory manager should have been initialized at this point.
	AK_StreamMgrNotInitialized     = 75,         ///< The stream manager should have been initialized at this point.
	AK_SSEInstructionsNotSupported = 76,         ///< The machine does not support SSE instructions (required on PC).
	AK_Busy                        = 77,         ///< The system is busy and could not process the request.
	AK_UnsupportedChannelConfig    = 78,         ///< Channel configuration is not supported in the current execution context.
	AK_PluginMediaNotAvailable     = 79,         ///< Plugin media is not available for effect.
	AK_MustBeVirtualized           = 80,         ///< Sound was Not Allowed to play.
	AK_CommandTooLarge             = 81,         ///< SDK command is too large to fit in the command queue.
	AK_RejectedByFilter            = 82,         ///< A play request was rejected due to the MIDI filter parameters.
	AK_InvalidCustomPlatformName   = 83,         ///< Detecting incompatibility between Custom platform of banks and custom platform of connected application
	AK_DLLCannotLoad               = 84,         ///< Plugin DLL could not be loaded, either because it is not found or one dependency is missing.
	AK_DLLPathNotFound             = 85,         ///< Plugin DLL search path could not be found.
	AK_NoJavaVM                    = 86,         ///< No Java VM provided in AkInitSettings.
	AK_OpenSLError                 = 87,         ///< OpenSL returned an error.  Check error log for more details.
	AK_PluginNotRegistered         = 88,         ///< Plugin is not registered.  Make sure to implement a AK::PluginRegistration class for it and use AK_STATIC_LINK_PLUGIN in the game binary.
	AK_DataAlignmentError          = 89,         ///< A pointer to audio data was not aligned to the platform's required alignment (check AkTypes.h in the platform-specific folder)
	AK_DeviceNotCompatible         = 90,         ///< Incompatible Audio device.
	AK_DuplicateUniqueID           = 91,         ///< Two Wwise objects share the same ID.
	AK_InitBankNotLoaded           = 92,         ///< The Init bank was not loaded yet, the sound engine isn't completely ready yet.
	AK_DeviceNotFound              = 93,         ///< The specified device ID does not match with any of the output devices that the sound engine is currently using.
	AK_PlayingIDNotFound           = 94,         ///< Calling a function with a playing ID that is not known.
	AK_InvalidFloatValue           = 95,         ///< One parameter has a invalid float value such as NaN, INF or FLT_MAX.
	AK_FileFormatMismatch          = 96,         ///< Media file format unexpected
	AK_NoDistinctListener          = 97,         ///< No distinct listener provided for AddOutput
	AK_ResourceInUse               = 99,         ///< Resource is in use and cannot be released.
	AK_InvalidBankType             = 100,        ///< Invalid bank type. The bank type was either supplied through a function call (e.g. LoadBank) or obtained from a bank loaded from memory.
	AK_AlreadyInitialized          = 101,        ///< Init() was called but that element was already initialized.
	AK_NotInitialized              = 102,        ///< The component being used is not initialized. Most likely AK::SoundEngine::Init() was not called yet, or AK::SoundEngine::Term was called too early.
	AK_FilePermissionError         = 103,        ///< The file access permissions prevent opening a file.
	AK_UnknownFileError            = 104,        ///< Rare file error occured, as opposed to AK_FileNotFound or AK_FilePermissionError. This lumps all unrecognized OS file system errors.
	AK_TooManyConcurrentOperations = 105,        ///< When using StdStream, file operations can be blocking or not. When not blocking, operations need to be synchronized externally properly. If not, this error occurs.
	AK_InvalidFileSize             = 106,        ///< The file requested was found and opened but is either 0 bytes long or not the expected size. This usually point toward a Low Level IO Hook implementation error.
	AK_Deferred                    = 107,        ///< Returned by functions to indicate to the caller the that the operation is done asynchronously. Used by Low Level IO Hook implementations when async operation are suppored by the hardware.
	AK_FilePathTooLong             = 108,        ///< The combination of base path and file name exceeds maximum buffer lengths.
	AK_InvalidState                = 109,        ///< This method should not be called when the object is in its current state.

	AKRESULT_Last                                ///< End of enum, invalid value.
};
typedef AkUInt32 AKRESULT_t;

/// Game sync group type
enum AkGroupType
{
	// should stay set as Switch = 0 and State = 1
	AkGroupType_Switch	= 0, ///< Type switch
	AkGroupType_State	= 1  ///< Type state
};
typedef AkUInt8 AkGroupType_t;

enum AkAudioDeviceState
{
	AkDeviceState_Unknown = 0,         ///< The audio device state is unknown or invalid.
	AkDeviceState_Active = 1 << 0,	   ///< The audio device is active That is, the audio adapter that connects to the endpoint device is present and enabled.
	AkDeviceState_Disabled = 1 << 1,   ///< The audio device is disabled.
	AkDeviceState_NotPresent = 1 << 2, ///< The audio device is not present because the audio adapter that connects to the endpoint device has been removed from the system.
	AkDeviceState_Unplugged = 1 << 3,  ///< The audio device is unplugged.
	AkDeviceState_Last,				   ///< End of enum, invalid value.

	AkDeviceState_All = AkDeviceState_Active | AkDeviceState_Disabled | AkDeviceState_NotPresent | AkDeviceState_Unplugged, ///< Includes audio devices in all states.
};
typedef AkUInt8 AkAudioDeviceState_t;

/// Nature of the connection binding an input to a bus.
enum AkConnectionType
{
	ConnectionType_Direct = 0x0,			///< Direct (main, dry) connection.
	ConnectionType_GameDefSend = 0x1,		///< Connection by an Automatic Room Send.
	ConnectionType_UserDefSend = 0x2,		///< Connection by a user-defined send.
	ConnectionType_ReflectionsSend = 0x3,	///< Connection by a early reflections send.
	ConnectionType_Last,					///< End of enum, invalid value.
};
typedef AkUInt8 AkConnectionType_t;

/// Curve types of the Attenuation Editor.
enum AkAttenuationCurveType
{
	AttenuationCurveID_VolumeDry = 0,       ///< The distance-driven dry volume curve.
	AttenuationCurveID_VolumeAuxGameDef,    ///< The distance-driven game-defined auxiliary send curve.
	AttenuationCurveID_VolumeAuxUserDef,    ///< The distance-driven user-defined auxiliary send curve.
	AttenuationCurveID_LowPassFilter,       ///< The distance-driven low-pass filter (pre send) curve.
	AttenuationCurveID_HighPassFilter,      ///< The distance-driven high-pass filter (pre send) curve.
	AttenuationCurveID_HighShelf,			///< The distance-driven high shelf filter curve.
	AttenuationCurveID_Spread,              ///< The distance-driven Spread curve.
	AttenuationCurveID_Focus,               ///< The distance-driven Focus curve.
	AttenuationCurveID_ObstructionVolume,   ///< The obstruction-driven volume curve.
	AttenuationCurveID_ObstructionLPF,      ///< The obstruction-driven low-pass filter curve.
	AttenuationCurveID_ObstructionHPF,      ///< The obstruction-driven high-pass filter curve.
	AttenuationCurveID_ObstructionHSF,       ///< The obstruction-driven high shelf filter curve.
	AttenuationCurveID_OcclusionVolume,     ///< The occlusion-driven volume curve.
	AttenuationCurveID_OcclusionLPF,        ///< The occlusion-driven low-pass filter curve.
	AttenuationCurveID_OcclusionHPF,        ///< The occlusion-driven high-pass filter curve.
	AttenuationCurveID_OcclusionHSF,        ///< The occlusion-driven high shelf filter curve.
	AttenuationCurveID_DiffractionVolume,   ///< The diffraction-driven volume curve.
	AttenuationCurveID_DiffractionLPF,      ///< The diffraction-driven low-pass filter curve.
	AttenuationCurveID_DiffractionHPF,      ///< The diffraction-driven high-pass filter curve.
	AttenuationCurveID_DiffractionHSF,		///< The diffraction-driven high shelf filter curve.
	AttenuationCurveID_TransmissionVolume,  ///< The transmission-driven volume curve.
	AttenuationCurveID_TransmissionLPF,     ///< The transmission-driven low-pass filter curve.
	AttenuationCurveID_TransmissionHPF,     ///< The transmission-driven high-pass filter curve.
	AttenuationCurveID_TransmissionHSF,		///< The transmission-driven high shelf filter curve.

	AttenuationCurveID_MaxCount,            ///< The maximum number of curve types.

	AttenuationCurveID_Project = 254,       ///< Symbol for "Use Project".
	AttenuationCurveID_None = 255           ///< Symbol for "None".
};
typedef AkUInt8 AkAttenuationCurveType_t;

/// Curve interpolation types
enum AkCurveInterpolation
{
	//DONT GO BEYOND 15! (see below for details)
	//Curves from 0 to LastFadeCurve NEED TO BE A MIRROR IMAGE AROUND LINEAR (eg. Log3 is the inverse of Exp3)
	AkCurveInterpolation_Log3           = 0, ///< Log3
	AkCurveInterpolation_Sine           = 1, ///< Sine
	AkCurveInterpolation_Log1           = 2, ///< Log1
	AkCurveInterpolation_InvSCurve      = 3, ///< Inversed S Curve
	AkCurveInterpolation_Linear         = 4, ///< Linear (Default)
	AkCurveInterpolation_SCurve         = 5, ///< S Curve
	AkCurveInterpolation_Exp1           = 6, ///< Exp1
	AkCurveInterpolation_SineRecip      = 7, ///< Reciprocal of sine curve
	AkCurveInterpolation_Exp3           = 8, ///< Exp3
	AkCurveInterpolation_LastFadeCurve  = 8, ///< Update this value to reflect last curve available for fades
	AkCurveInterpolation_Constant       = 9, ///< Constant ( not valid for fading values )

	AkCurveInterpolation_Last,               ///< End of enum, invalid value.
	//DONT GO BEYOND 15! The value is stored on 5 bits,
	//but we can use only 4 bits for the actual values, keeping
	//the 5th bit at 0 to void problems when the value is
	//expanded to 32 bits.
};
typedef AkUInt8 AkCurveInterpolation_t;
#define AKCURVEINTERPOLATION_NUM_STORAGE_BIT 5 ///< Internal storage restriction, for internal use only.

/// Bank types
enum AkBankTypeEnum
{
	AkBankType_User		= AKCODECID_BANK,			///< User-defined bank.
	AkBankType_Event	= AKCODECID_BANK_EVENT,		///< Bank generated for one event.
	AkBankType_Bus		= AKCODECID_BANK_BUS,		///< Bank generated for one bus or aux bus.
	AkBankType_Last									///< End of enum, invalid value.
};
typedef AkUInt8 AkBankTypeEnum_t;

#define AK_PANNER_NUM_STORAGE_BITS 3
/// Speaker panning type: type of panning logic when object is not 3D spatialized (i.e. when Ak3DSpatializationMode is AK_SpatializationMode_None).
enum AkSpeakerPanningType
{
	AK_DirectSpeakerAssignment = 0, ///< No panning: route to matching channels between input and output.
	AK_BalanceFadeHeight       = 1, ///< Balance-Fade-Height: Traditional "box" or "car"-like panner.
	AK_SteeringPanner          = 2, ///< Steering panner.
	AkSpeakerPanning_Last			///< End of enum, invalid value.
};
typedef AkUInt8 AkSpeakerPanningType_t;

#define AK_POSSOURCE_NUM_STORAGE_BITS 3
/// 3D position type: defines what acts as the emitter position for computing spatialization against the listener. Used when Ak3DSpatializationMode is AK_SpatializationMode_PositionOnly or AK_SpatializationMode_PositionAndOrientation.
enum Ak3DPositionType
{
	AK_3DPositionType_Emitter = 0,					///< 3D spatialization is computed directly from the emitter game object position.
	AK_3DPositionType_EmitterWithAutomation = 1,	///< 3D spatialization is computed from the emitter game object position, translated by user-defined automation.
	AK_3DPositionType_ListenerWithAutomation = 2,	///< 3D spatialization is computed from the listener game object position, translated by user-defined automation.
	AK_3DPositionType_Last							///< End of enum, invalid value.
};
typedef AkUInt8 Ak3DPositionType_t;

/// Headphone / speakers panning rules
enum AkPanningRule
{
	AkPanningRule_Speakers		= 0,	///< Left and right positioned 60 degrees apart (by default - see AK::SoundEngine::GetSpeakerAngles()).
	AkPanningRule_Headphones 	= 1,	///< Left and right positioned 180 degrees apart.
	AkPanningRule_Last					///< End of enum, invalid value.
};
typedef AkUInt8 AkPanningRule_t;

#define AK_SPAT_NUM_STORAGE_BITS 3
/// 3D spatialization mode.
enum Ak3DSpatializationMode
{
	AK_SpatializationMode_None = 0,						///< No spatialization
	AK_SpatializationMode_PositionOnly = 1,				///< Spatialization based on emitter position only.
	AK_SpatializationMode_PositionAndOrientation = 2,	///< Spatialization based on both emitter position and emitter orientation.
	AK_SpatializationMode_Last							///< End of enum, invalid value.
};
typedef AkUInt8 Ak3DSpatializationMode_t;

#define AK_MAX_BITS_METERING_FLAGS	(5)	// Keep in sync with AkMeteringFlags.

/// Metering flags. Used for specifying bus metering, through AK::SoundEngine::RegisterBusVolumeCallback() or AK::IAkMixerPluginContext::SetMeteringFlags().
enum AkMeteringFlags
{
	AK_NoMetering				= 0,			///< No metering.
	AK_EnableBusMeter_Peak		= 1 << 0,		///< Enable computation of peak metering.
	AK_EnableBusMeter_TruePeak	= 1 << 1,		///< Enable computation of true peak metering (most CPU and memory intensive).
	AK_EnableBusMeter_RMS		= 1 << 2,		///< Enable computation of RMS metering.
	// 1 << 3 is reserved.
	AK_EnableBusMeter_KPower	= 1 << 4,		///< Enable computation of K-weighted power metering (used as a basis for computing loudness, as defined by ITU-R BS.1770).
	AK_EnableBusMeter_3DMeter	= 1 << 5,		///< Enable computation of data necessary to render a 3D visualization of volume distribution over the surface of a sphere.
	AK_EnableBusMeter_Last						///< End of enum, invalid value.
};
typedef AkUInt8 AkMeteringFlags_t;

/// Plug-in type.
/// \sa
/// - AkPluginInfo
enum AkPluginType
{
	AkPluginTypeNone = 0,	///< Unknown/invalid plug-in type.
	AkPluginTypeCodec = 1,	///< Compressor/decompressor plug-in (allows support for custom audio file types).
	AkPluginTypeSource = 2,	///< Source plug-in: creates sound by synthesis method (no input, just output).
	AkPluginTypeEffect = 3,	///< Effect plug-in: applies processing to audio data.
	//AkPluginTypeMotionDevice = 4,	///< Motion Device plug-in: feeds movement data to devices. Deprecated by Motion refactor.
	//AkPluginTypeMotionSource = 5,	///< Motion Device source plug-in: feeds movement data to device busses. Deprecated by Motion refactor.
	AkPluginTypeMixer = 6,	///< Mixer plug-in: mix voices at the bus level.
	AkPluginTypeSink = 7,	///< Sink plug-in: implement custom sound engine end point.
	AkPluginTypeGlobalExtension = 8,	///< Global Extension plug-in: (e.g. Spatial Audio, Interactive Music)
	AkPluginTypeMetadata = 9,	///< Metadata plug-in: applies object-based processing to audio data

	AkPluginType_Last,			///< End of enum, invalid value.
	AkPluginTypeMask = 0xf 	///< Plug-in type mask is 4 bits.
};
typedef AkUInt8 AkPluginType_t;

enum AkNodeType
{
	AkNodeType_Default,
	AkNodeType_Bus,
	AkNodeType_AudioDevice,
	AkNodeType_Last			///< End of enum, invalid value.
};
typedef AkUInt8 AkNodeType_t;

/// AkMultiPositionType.
/// \aknote
/// - If a sound has diffraction enabled, it is treated as <tt>AkMultiPositionType_MultiDirections</tt>. <tt>AkMultiPositionType_MultiSources</tt> is not supported in this case.
/// \endaknote
/// \sa
/// - AK::SoundEngine::SetMultiplePosition()
/// - AkCommand_SetMultiplePositions
/// - \ref soundengine_3dpositions_multiplepos
enum AkMultiPositionType
{
	AkMultiPositionType_SingleSource,       ///< Used for normal sounds, not expected to pass to AK::SoundEngine::SetMultiplePosition() (if done, only the first position will be used).
	AkMultiPositionType_MultiSources,       ///< Simulate multiple sources in one sound playing, adding volumes. For instance, all the torches on your level emitting using only one sound.
	AkMultiPositionType_MultiDirections,    ///< Simulate one sound coming from multiple directions. Useful for repositionning sounds based on wall openings or to simulate areas like forest or rivers ( in combination with spreading in the attenuation of the sounds ).

	AkMultiPositionType_Last                ///< End of enum, invalid value.
};
typedef AkUInt8 AkMultiPositionType_t;

/// Flags to independently set the position of the emitter or listener component on a game object.
enum AkSetPositionFlags
{
	AkSetPositionFlags_Emitter = 1 << 0,   ///< Only set the emitter component position.
	AkSetPositionFlags_Listener = 1 << 1,  ///< Only set the listener component position.

	AkSetPositionFlags_Default = (AkSetPositionFlags_Emitter | AkSetPositionFlags_Listener)  ///< Default: set both emitter and listener component positions.
};
typedef AkUInt8 AkSetPositionFlags_t;

// Type of operation for the listener commands.
enum AkListenerOp
{
	AkListenerOp_Set,     ///< Set the list of listeners
	AkListenerOp_Add,     ///< Add listeners
	AkListenerOp_Remove,  ///< Remove listeners
};
typedef AkUInt8 AkListenerOp_t;

/// Denotes actions that can be executed on nodes referenced by events in an action of type Play
/// \sa
/// - <tt>AK::SoundEngine::ExecuteActionOnEvent()</tt>
/// - <tt>AkCommand_ExecuteActionOnEvent</tt>
enum AkActionOnEventType
{
	AkActionOnEventType_Stop = 0,            ///< Stop
	AkActionOnEventType_Pause = 1,           ///< Pause
	AkActionOnEventType_Resume = 2,          ///< Resume
	AkActionOnEventType_Break = 3,           ///< Break
	AkActionOnEventType_ReleaseEnvelope = 4, ///< Release envelope
	AkActionOnEventType_Last                 ///< End of enum, invalid value.
};
typedef AkUInt8 AkActionOnEventType_t;

/// The AkDynamicSequenceType is specified when creating a new dynamic sequence.
/// 
/// In sample accurate mode, when a dynamic sequence item finishes playing and there is another item
/// pending in its playlist, the next sound will be stitched to the end of the ending sound. In this
/// mode, if there are one or more pending items in the playlist while the dynamic sequence is playing,
/// or if something is added to the playlist during the playback, the dynamic sequence
/// can remove the next item to be played from the playlist and prepare it for sample accurate playback before
/// the first sound is finished playing. This mechanism helps keep sounds sample accurate, but then
/// you might not be able to remove that next item from the playlist if required.
/// 
/// If your game requires the capability of removing the next to be played item from the
/// playlist at any time, then you should use the DynamicSequenceType_NormalTransition option  instead.
/// In this mode, you cannot ensure sample accuracy between sounds.
/// 
/// Note that a Stop or a Break will always prevent the next to be played sound from actually being played.
///
/// \sa
/// - AkCmd_DynamicSequence_Open
/// - AK::SoundEngine::DynamicSequence::Open
enum AkDynamicSequenceType
{
	AkDynamicSequenceType_SampleAccurate,     ///< Sample accurate mode
	AkDynamicSequenceType_NormalTransition,   ///< Normal transition mode, allows the entire playlist to be edited at all times.
	AkDynamicSequenceType_Last                ///< End of enum, invalid value.
};
typedef AkUInt8 AkDynamicSequenceType_t;

/// Denotes operations that can be executed on a Dynamic Sequence.
/// \sa
/// - AkCmd_DynamicSequence_Op
enum AkDynamicSequenceOp
{
	AkDynamicSequenceOp_Play,           // Start playback of a stopped sequence
	AkDynamicSequenceOp_Pause,          // Pause a sequence during playback. 
	AkDynamicSequenceOp_Resume,         // Resume playback of a paused sequence at the position where it was paused. 
	AkDynamicSequenceOp_Stop,           // Stop playback of a sequence.
	AkDynamicSequenceOp_Break,          // Stop playback of a sequence after the current item has finished playing.
	AkDynamicSequenceOp_Close,          // Signal the end of a sequence. Playback continues but no more items can be added after Close. The Dynamic Sequence will play until finished and then deallocate itself.

	AkDynamicSequenceOp_Last
};
typedef AkUInt8 AkDynamicSequenceOp_t;

/// Channel configuration type. 
enum AkChannelConfigType
{
	AK_ChannelConfigType_Anonymous = 0x0,   ///< Channel mask == 0 and channels are anonymous.
	AK_ChannelConfigType_Standard = 0x1,    ///< Channels must be identified with standard defines in AkSpeakerConfigs.	
	AK_ChannelConfigType_Ambisonic = 0x2,   ///< Ambisonics. Channel mask == 0 and channels follow standard ambisonic order.
	AK_ChannelConfigType_Objects = 0x3,     ///< Object-based configurations.
	AK_ChannelConfigType_Last,              ///< End of enum, invalid value.

	AK_ChannelConfigType_UseDeviceMain = 0xE,           ///< Special setting for bus objects to use the audio device main configuration.
	AK_ChannelConfigType_UseDevicePassthrough = 0xF     ///< Special setting for bus objects to use the audio device passthrough configuration.
};

/// Type of physical device being operated by Wwise Motion
enum AkMotionDeviceType
{
	AkMotionDeviceType_Controller = 0,  ///< Internal motors/actuators of a standard game controller
	AkMotionDeviceType_Mobile,          ///< Internal motors/actuators of mobile device (phone or tablet)

	AkMotionDeviceType_Last
};

/// Input data profiles for Wwise Motion
enum AkMotionInputProfile
{
	AkMotionInputProfile_GenericRumble = 0,        ///< Generic PCM for conversion to amplitude curve for low-resolution rumble
	AkMotionInputProfile_GenericLoResHaptics,      ///< Generic PCM for linear actuators supporting < 1KHz sample rates
	AkMotionInputProfile_GenericHiResHaptics,      ///< Generic PCM for linear actuators supporting >= 1KHz sample rates
	AkMotionInputProfile_MetaQuestHaptics,         ///< PCM haptics for Meta Quest controllers

	AkMotionInputProfile_Last
};

/// Rendering modes of the overall Sound Engine
enum AkEngineRenderingMode
{
	AkEngineRenderingMode_Online = 0,          ///< Regular operation, based on real audio sink cadence
	AkEngineRenderingMode_Offline = 1,         ///< No audio output, rendering as fast as possible, every operation is serialized (streaming, monitoring, bank loads, etc)
	AkEngineRenderingMode_OfflineThreaded = 2  ///< No audio output, rendering as fast as possible, but regular threaded operations still happen in threads. 
};
