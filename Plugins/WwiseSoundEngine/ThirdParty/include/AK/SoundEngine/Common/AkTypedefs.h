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

#include <AK/SoundEngine/Common/AkNumeralTypes.h>

typedef AkUInt32 AkUniqueID;          ///< Unique 32-bit ID
typedef AkUInt32 AkStateID;           ///< State ID
typedef AkUInt32 AkStateGroupID;      ///< State group ID
typedef AkUInt32 AkPlayingID;         ///< A unique identifier generated whenever a PostEvent is called (or when a Dynamic Sequence is created). This identifier serves as a handle to control and interact with individual playback instances, such as stopping a specific instance, and is the ID used in the EndOfEvent callback (see AkCallbackType).
typedef AkInt32  AkTimeMs;            ///< Time in ms
typedef AkUInt16 AkPortNumber;        ///< Port number
typedef AkReal32 AkPitchValue;        ///< Pitch value
typedef AkReal32 AkVolumeValue;       ///< Volume value( also apply to LFE )
typedef AkUInt64 AkGameObjectID;      ///< Game object ID
typedef AkReal32 AkLPFType;           ///< Low-pass filter type
typedef AkInt32  AkMemPoolId;         ///< Memory pool ID
typedef AkUInt32 AkPluginID;          ///< Source or effect plug-in ID
typedef AkUInt32 AkCodecID;           ///< Codec plug-in ID
typedef AkUInt32 AkAuxBusID;          ///< Auxilliary bus ID
typedef AkInt16  AkPluginParamID;     ///< Source or effect plug-in parameter ID
typedef AkInt8   AkPriority;          ///< Priority
typedef AkUInt16 AkDataCompID;        ///< Data compression format ID
typedef AkUInt16 AkDataTypeID;        ///< Data sample type ID
typedef AkUInt8  AkDataInterleaveID;  ///< Data interleaved state ID
typedef AkUInt32 AkSwitchGroupID;     ///< Switch group ID
typedef AkUInt32 AkSwitchStateID;     ///< Switch ID
typedef AkUInt32 AkRtpcID;            ///< Real time parameter control ID
typedef AkReal32 AkRtpcValue;         ///< Real time parameter control value
typedef AkUInt32 AkBankID;            ///< Run time bank ID
typedef AkUInt32 AkBankType;          ///< Run time bank type
typedef AkUInt32 AkFileID;            ///< Integer-type file identifier
typedef AkUInt32 AkDeviceID;          ///< I/O device ID
typedef AkUInt32 AkTriggerID;         ///< Trigger ID
typedef AkUInt32 AkArgumentValueID;   ///< Argument value ID
typedef AkUInt32 AkChannelMask;       ///< Channel mask (similar to extensibleWavFormat). Bit values are defined in AkSpeakerConfig.h.
typedef AkUInt32 AkModulatorID;       ///< Modulator ID
typedef AkUInt32 AkAcousticTextureID; ///< Acoustic Texture ID
typedef AkUInt32 AkImageSourceID;     ///< Image Source ID
typedef AkUInt64 AkOutputDeviceID;    ///< Audio Output device ID
typedef AkUInt32 AkPipelineID;        ///< Unique node (bus, voice) identifier for profiling.
typedef AkUInt32 AkRayID;             ///< Unique (per emitter) identifier for an emitter-listener ray.
typedef AkUInt64 AkAudioObjectID;     ///< Audio Object ID
typedef AkUInt32 AkJobType;           ///< Job type identifier
typedef AkUInt64 AkCacheID;           ///< Stream cache block ID.
typedef AkUInt16 AkVertIdx;           ///< Index of vertex in SA geometry
typedef AkUInt16 AkTriIdx;            ///< Index of triangle in SA geometry
typedef AkUInt16 AkSurfIdx;           ///< Index of surface in SA geometry

// Volume vector. Access each element with the standard bracket [] operator.
typedef AkReal32* AkSpeakerVolumesMatrixPtr;

// Volume matrix. Access each input channel vector with AK::SpeakerVolumes::Matrix::GetChannel().
typedef AkReal32* AkSpeakerVolumesVectorPtr;

///< Constant volume vector. Access each element with the standard bracket [] operator.
typedef const AkReal32 * AkSpeakerVolumesConstVectorPtr;

///< Constant volume matrix. Access each input channel vector with AK::SpeakerVolumes::Matrix::GetChannel().
typedef const AkReal32 * AkSpeakerVolumesConstMatrixPtr;

///< Opaque data structure for storing a collection of external sources. Refer to API in AK/SoundEngine/Common/AkExternalSourceArray.h to learn how to manipulate this data structure.
typedef void* AkExternalSourceArray;

#ifdef __cplusplus
namespace AK
{
/// Multi-channel volume definitions and services.
namespace SpeakerVolumes
{
	using VectorPtr = AkSpeakerVolumesMatrixPtr;
	using MatrixPtr = AkSpeakerVolumesVectorPtr;
	using ConstVectorPtr = AkSpeakerVolumesConstVectorPtr;
	using ConstMatrixPtr = AkSpeakerVolumesConstMatrixPtr;
}
}
#endif // __cplusplus
