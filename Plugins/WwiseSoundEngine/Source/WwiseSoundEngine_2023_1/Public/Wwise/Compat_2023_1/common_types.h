/*******************************************************************************
The content of this file includes portions of the proprietary AUDIOKINETIC Wwise
Technology released in source code form as part of the game integration package.
The content of this file may not be used without valid licenses to the
AUDIOKINETIC Wwise Technology.
Note that the use of the game engine is subject to the Unreal(R) Engine End User
License Agreement at https://www.unrealengine.com/en-US/eula/unreal
 
License Usage
 
Licensees holding valid licenses to the AUDIOKINETIC Wwise Technology may use
this file in accordance with the end user license agreement provided with the
software or, alternatively, in accordance with the terms contained
in a written agreement between you and Audiokinetic Inc.
Copyright (c) 2026 Audiokinetic Inc.
*******************************************************************************/

#pragma once

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
	AkMultiPositionType_SingleSource,		///< Used for normal sounds, not expected to pass to AK::SoundEngine::SetMultiplePosition() (if done, only the first position will be used).
	AkMultiPositionType_MultiSources,		///< Simulate multiple sources in one sound playing, adding volumes. For instance, all the torches on your level emitting using only one sound.
	AkMultiPositionType_MultiDirections,	///< Simulate one sound coming from multiple directions. Useful for repositionning sounds based on wall openings or to simulate areas like forest or rivers ( in combination with spreading in the attenuation of the sounds ).
	AkMultiPositionType_Last				///< End of enum, invalid value.
};
typedef enum AkMultiPositionType ak_multi_position_type_t;

enum AkActionOnEventType
{
	AkActionOnEventType_Stop = 0,            ///< Stop
	AkActionOnEventType_Pause = 1,           ///< Pause
	AkActionOnEventType_Resume = 2,          ///< Resume
	AkActionOnEventType_Break = 3,           ///< Break
	AkActionOnEventType_ReleaseEnvelope = 4, ///< Release envelope
	AkActionOnEventType_Last                 ///< End of enum, invalid value.
};
typedef enum AkActionOnEventType ak_action_on_event_type_t;
