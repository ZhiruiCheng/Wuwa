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

#include <AK/AkPlatforms.h>
#include <AK/SoundEngine/Common/AkSoundEngineExport.h>
#include <AK/SoundEngine/Common/AkNumeralTypes.h>
#include <AK/SoundEngine/Common/AkTypedefs.h>
#include <AK/SoundEngine/Common/AkEnums.h>
#include <AK/SoundEngine/Common/AkSpeakerConfig.h>

#if defined(__LP64__) || defined(_LP64) || defined(_M_AMD64) || defined(_M_ARM64) || defined(AK_POINTER_64)
#define AK_WPTR(__t__, __n__) __t__ __n__
#else
#define AK_WPTR(__t__, __n__) \
	__t__ __n__; \
	AkUInt32 __##__n__##_padding

#endif

#define AkMax(x1, x2)	(((x1) > (x2))? (x1): (x2))
#define AkMin(x1, x2)	(((x1) < (x2))? (x1): (x2))
#define AkClamp(x, min, max)  ((x) < (min)) ? (min) : (((x) > (max) ? (max) : (x)))

/// Configured audio settings
struct AkAudioSettings
{
	AkUInt32			uNumSamplesPerFrame;		///< Number of samples per audio frame (256, 512, 1024 or 2048).
	AkUInt32			uNumSamplesPerSecond;		///< Number of samples per second.
};

struct AkDeviceDescription
{
	AkUInt32 idDevice;                        ///< Device ID for Wwise. This is the same as what is returned from AK::GetDeviceID and AK::GetDeviceIDFromName. Use it to specify the main device in AkPlatformInitSettings.idAudioDevice or in AK::SoundEngine::AddSecondaryOutput. 
	AkOSChar deviceName[AK_MAX_PATH];         ///< The user-friendly name for the device.
	enum AkAudioDeviceState deviceStateMask;  ///< Bitmask used to filter the device based on their state.
	bool isDefaultDevice;                     ///< Identify default device. Always false when not supported.
};

/// Custom data about a Wwise Motion output device instance
///
/// This data can be retrieved via the \c customData member of AkOutputDeviceInfo.
struct AkMotionDeviceData
{
	enum AkMotionDeviceType deviceType;         ///< Type of motion device
	enum AkMotionInputProfile inputProfile;     ///< Indicates the ideal input data profile for this Motion device instance
};

/// Structure containing information about system-level support for 3D audio.
/// "3D Audio" refers to a system's ability to position sound sources in a virtual 3D space, pan them accordingly on a range of physical speakers, and produce a binaural mix where appropriate.
/// We prefer "3D Audio" to "Spatial" to avoid ambiguity with spatial audio, which typically involves sound propagation and environment effects.
struct Ak3DAudioSinkCapabilities
{
	struct AkChannelConfig channelConfig;                         ///< Channel configuration of the main mix.
	AkUInt32        uMaxSystemAudioObjects;                       ///< Maximum number of System Audio Objects that can be active concurrently. A value of zero indicates the system does not support this feature.
	AkUInt32        uAvailableSystemAudioObjects;                 ///< How many System Audio Objects can currently be sent to the sink. This value can change at runtime depending on what is playing. Can never be higher than uMaxSystemAudioObjects.
	bool            bPassthrough;                                 ///< Separate  pass-through mix is supported.
	bool            bMultiChannelObjects;                         ///< Can handle multi-channel objects

#ifdef __cplusplus
	AkForceInline bool operator!=(const Ak3DAudioSinkCapabilities& in_other) const
	{
		return channelConfig != in_other.channelConfig
			|| uMaxSystemAudioObjects != in_other.uMaxSystemAudioObjects
			|| uAvailableSystemAudioObjects!= in_other.uAvailableSystemAudioObjects
			|| bPassthrough != in_other.bPassthrough
			|| bMultiChannelObjects != in_other.bMultiChannelObjects;
	}
#endif
};

/// Information about an output device instance.
/// 
/// This data can be retrieved by plug-ins via AK::IAkPluginContextBase::GetOutputDeviceInfo.
struct AkOutputDeviceInfo
{
	AkPluginID pluginID;                            ///< Full plugin ID, including company ID and plugin type. See AKMAKECLASSID macro.
	AkUniqueID audioDeviceShareset;                 ///< ID of shareset for this output device
	AkUInt32 idDevice;                              ///< Device ID that was specified in the Output Settings for this output device
	struct AkChannelConfig channelConfig;           ///< Channel configuration of the output device
	struct Ak3DAudioSinkCapabilities capabilities;  ///< 3D Audio capabilities of the output device instance
	void* customData;                               ///< Custom data containing additional information about the device. This is plug-in implementation-defined. For Wwise Motion, this can be cast to AkMotionDeviceData.
};

/// Obstruction/occlusion pair for a position
struct AkObstructionOcclusionValues
{
	AkReal32 occlusion;    ///< OcclusionLevel: [0.0f..1.0f]
	AkReal32 obstruction;  ///< ObstructionLevel: [0.0f..1.0f]
};

/// Auxiliary bus sends information per game object per given auxiliary bus.
struct AkAuxSendValue
{
	AkGameObjectID listenerID;	///< Game object ID of the listener associated with this send. Use AK_INVALID_GAME_OBJECT as a wildcard to set the auxiliary send to all connected listeners (see AK::SoundEngine::SetListeners).
	AkAuxBusID auxBusID;		///< Auxiliary bus ID.
	AkReal32 fControlValue;		///< A value in the range [0.0f:16.0f] ( -∞ dB to +24 dB).
	///< Represents the attenuation or amplification factor applied to the volume of the sound going through the auxiliary bus. 
	///< A value greater than 1.0f will amplify the sound. 
};

/// This structure allows the game to provide audio files to fill the external sources. See \ref AK::SoundEngine::PostEvent
/// You can specify a streaming file or a file in-memory, regardless of the "Stream" option in the Wwise project.
/// The recommended format is a fully formed WEM file, as converted by Wwise. WAV files are also supported, but only in file streaming mode and with some limitations (e.g. PCM samples only, exotic channel configurations may not work as expected)
/// \akwarning
/// Make sure that only one of szFile, pInMemory or idFile is non-null. if both idFile and szFile are set, idFile is passed to low-level IO and szFile is used as stream name (for profiling purposes).
/// \endakwarning
/// \akwarning
/// When using the in-memory file (pInMemory & uiMemorySize), it is the responsibility of the game to ensure the memory stays valid for the entire duration of the playback. 
/// You can achieve this by using the AK_EndOfEvent callback to track when the Event ends.
/// \endakwarning
/// \sa
/// - AK::SoundEngine::PostEvent
struct AkExternalSourceInfo
{
#ifdef __cplusplus
	/// Default constructor.
	AkExternalSourceInfo()
	{
		iExternalSrcCookie = 0;
		idCodec = 0;
		szFile = nullptr;
		pInMemory = nullptr;
		uiMemorySize = 0;
		idFile = 0;
	}

	/// Constructor: specify source by memory.
	AkExternalSourceInfo(
		void* in_pInMemory,				///< Pointer to the in-memory file.
		AkUInt32 in_uiMemorySize,		///< Size of data.
		AkUInt32 in_iExternalSrcCookie,	///< Cookie.
		AkCodecID in_idCodec			///< Codec ID.
	)
	{
		iExternalSrcCookie = in_iExternalSrcCookie;
		idCodec = in_idCodec;
		szFile = nullptr;
		pInMemory = in_pInMemory;
		uiMemorySize = in_uiMemorySize;
		idFile = 0;
	}

	/// Constructor: specify source by streaming file name.
	AkExternalSourceInfo(
		const char* in_pszFileName,		///< File name.
		AkUInt32 in_iExternalSrcCookie,	///< Cookie.
		AkCodecID in_idCodec			///< Codec ID.
	)
	{
		iExternalSrcCookie = in_iExternalSrcCookie;
		idCodec = in_idCodec;
		szFile = in_pszFileName;
		pInMemory = nullptr;
		uiMemorySize = 0;
		idFile = 0;
	}

	/// Constructor: specify source by streaming file ID.
	AkExternalSourceInfo(
		AkFileID in_idFile,				///< File ID.
		AkUInt32 in_iExternalSrcCookie,	///< Cookie.
		AkCodecID in_idCodec			///< Codec ID.
	)
	{
		iExternalSrcCookie = in_iExternalSrcCookie;
		idCodec = in_idCodec;
		szFile = nullptr;
		pInMemory = nullptr;
		uiMemorySize = 0;
		idFile = in_idFile;
	}
#endif

	AkUInt32 iExternalSrcCookie;    ///< Cookie identifying the source, given by hashing the name of the source given in the project.  See \ref AK::SoundEngine::GetIDFromString. \aknote If an event triggers the playback of more than one external source, they must be named uniquely in the project therefore have a unique cookie) in order to tell them apart when filling the AkExternalSourceInfo structures. \endaknote
	AkCodecID idCodec;              ///< Codec ID for the file.  One of the audio formats defined in AkTypes.h (AKCODECID_XXX)
	AK_WPTR(const char*, szFile);   ///< UTF-8 File path for the source.  If not NULL, the source will be streaming from disk. Set pInMemory to NULL. If idFile is set, this field is used as stream name (for profiling purposes). The only file formats accepted are a fully formed WEM file, as converted by Wwise, or a standard WAV file.
	AK_WPTR(void*, pInMemory);      ///< Pointer to the in-memory file.  If not NULL, the source will be read from memory. Set szFile and idFile to NULL. The only file format accepted is a fully formed WEM file, as converted by Wwise.
	AkUInt32 uiMemorySize;          ///< Size of the data pointed by pInMemory
	AkFileID idFile;                ///< File ID.  If not zero, the source will be streaming from disk.  This ID can be anything.  Note that you must override the low-level IO to resolve this ID to a real file.  See \ref streamingmanager_lowlevel for more information on overriding the Low Level IO.
};

/// Volume ramp specified by end points "previous" and "next".
struct AkRamp
{
#ifdef __cplusplus
	AkRamp() : fPrev( 1.f ), fNext( 1.f ) {}
	AkRamp( AkReal32 in_fPrev, AkReal32 in_fNext ) : fPrev( in_fPrev ), fNext( in_fNext ) {}
	AkRamp & operator*=(const AkRamp& in_rRhs) { fPrev *= in_rRhs.fPrev; fNext *= in_rRhs.fNext; return *this; }
#endif

	AkReal32 fPrev;
	AkReal32 fNext;	
};
#ifdef __cplusplus
inline AkRamp operator*(const AkRamp& in_rLhs, const AkRamp& in_rRhs) 
{
	AkRamp result(in_rLhs);
	result *= in_rRhs;
	return result;
}
#endif

/// Platform-independent initialization settings of output devices.
struct AkOutputSettings
{
#ifdef __cplusplus
	AkOutputSettings() :
		audioDeviceShareset(AK_INVALID_UNIQUE_ID),
		idDevice(0),
		ePanningRule(AkPanningRule_Speakers),
		channelConfig() {};

	AkOutputSettings(const char* in_szDeviceShareSet, AkUniqueID in_idDevice = AK_INVALID_UNIQUE_ID, AkChannelConfig in_channelConfig = AkChannelConfig(), AkPanningRule in_ePanning = AkPanningRule_Speakers);

#ifdef AK_SUPPORT_WCHAR
	AkOutputSettings(const wchar_t* in_szDeviceShareSet, AkUniqueID in_idDevice = AK_INVALID_UNIQUE_ID, AkChannelConfig in_channelConfig = AkChannelConfig(), AkPanningRule in_ePanning = AkPanningRule_Speakers);
#endif
#endif // __cplusplus

	/// Unique ID of a custom audio device to be used. Custom audio devices are defined in the Audio Device Shareset section of the Wwise project.
	/// If you want to output normally through the output device defined on the Master Bus in your project, leave this field to its default value (AK_INVALID_UNIQUE_ID, or value 0).
	/// Typical usage: AkInitSettings.eOutputSettings.audioDeviceShareset = AK::SoundEngine::GetIDFromString("InsertYourAudioDeviceSharesetNameHere");
	/// \sa <tt>\ref AK::SoundEngine::GetIDFromString()</tt>
	/// \sa \ref soundengine_plugins_audiodevices
	/// \sa \ref integrating_secondary_outputs
	/// \sa \ref default_audio_devices
	AkUniqueID audioDeviceShareset;

	/// Device specific identifier, when multiple devices of the same type are possible.  If only one device is possible, leave to 0.
	/// \sa \ref obtaining_device_id
	AkUInt32 idDevice;

	/// Rule for 3D panning of signals routed to a stereo bus. In AkPanningRule_Speakers mode, the angle of the front loudspeakers 
	/// (uSpeakerAngles[0]) is used. In AkPanningRule_Headphones mode, the speaker angles are superseded by constant power panning
	/// between two virtual microphones spaced 180 degrees apart.
	enum AkPanningRule ePanningRule;

	/// Channel configuration for this output. Call AkChannelConfig::Clear() to let the engine use the default output configuration.  
	/// Hardware might not support the selected configuration.
	struct AkChannelConfig channelConfig;
};

#ifdef __cplusplus

class IAkSoftwareCodec;
class IAkFileCodec;
class IAkGrainCodec;
/// Registered file source creation function prototype.
AK_CALLBACK( IAkSoftwareCodec*, AkCreateFileSourceCallback )( void* in_pCtx );
/// Registered bank source node creation function prototype.
AK_CALLBACK( IAkSoftwareCodec*, AkCreateBankSourceCallback )( void* in_pCtx );
/// Registered FileCodec creation function prototype.
AK_CALLBACK( IAkFileCodec*, AkCreateFileCodecCallback )();
/// Registered IAkGrainCodec creation function prototype.
AK_CALLBACK( IAkGrainCodec*, AkCreateGrainCodecCallback )();

struct AkCodecDescriptor
{
	AkCreateFileSourceCallback pFileSrcCreateFunc;       // File VPL source.
	AkCreateBankSourceCallback pBankSrcCreateFunc;       // Bank VPL source.
	AkCreateFileCodecCallback pFileCodecCreateFunc;      // FileCodec utility.
	AkCreateGrainCodecCallback pGrainCodecCreateFunc;    // GrainCodec utility.
};

struct WwiseObjectIDext
{
public:

	bool operator == ( const WwiseObjectIDext& in_rOther ) const
	{
		return in_rOther.id == id && in_rOther.bIsBus == bIsBus;
	}

	AkNodeType GetType()
	{
		return bIsBus ? AkNodeType_Bus : AkNodeType_Default;
	}

	AkUniqueID	id;
	bool		bIsBus;
};


struct WwiseObjectID : public WwiseObjectIDext
{
	WwiseObjectID()
	{
		id = AK_INVALID_UNIQUE_ID;
		bIsBus = false;
	}

	WwiseObjectID( AkUniqueID in_ID )
	{
		id = in_ID;
		bIsBus = false;
	}

	WwiseObjectID( AkUniqueID in_ID, bool in_bIsBus )
	{
		id = in_ID;
		bIsBus = in_bIsBus;
	}

	WwiseObjectID( AkUniqueID in_ID, AkNodeType in_eNodeType )
	{
		id = in_ID;
		bIsBus = in_eNodeType == AkNodeType_Bus;
	}
};

/// Type for a point in an RTPC or Attenuation curve.
template< class VALUE_TYPE >
struct AkGraphPointBase
{
	AkGraphPointBase() = default;

	AkGraphPointBase(AkReal32 in_from, VALUE_TYPE in_to, AkCurveInterpolation in_interp)
		: From(in_from)
		, To(in_to)
		, Interp(in_interp)
	{}

	AkReal32				From;		///< Represents the value on the X axis.
	VALUE_TYPE				To;	    	///< Represents the value on the Y axis.
	AkCurveInterpolation	Interp;		///< The shape of the interpolation curve between this point and the next.

	bool operator==(const AkGraphPointBase& other) const
	{
		return From == other.From && To == other.To && Interp == other.Interp;
}
	bool operator!=(const AkGraphPointBase& other) const
	{
		return !(*this == other);
	}
};

// Every point of a conversion graph
typedef AkGraphPointBase<AkReal32> AkRTPCGraphPoint;

/// Public data structures for converted file format.
namespace AkFileParser
{
#pragma pack(push, 1)
	/// Analyzed envelope point.
	struct EnvelopePoint
	{
		AkUInt32 uPosition;		/// Position of this point in samples at the source rate.
		AkUInt16 uAttenuation;	/// Approximate _attenuation_ at this location relative to this source's maximum, in dB (absolute value).
	};
#pragma pack(pop)
}
#endif

#ifndef AK_ASYNC_OPEN_DEFAULT
#define AK_ASYNC_OPEN_DEFAULT	(false)				///< Refers to asynchronous file opening in default low-level IO.
#endif

#ifndef AK_COMM_DEFAULT_DISCOVERY_PORT
#define AK_COMM_DEFAULT_DISCOVERY_PORT 24024	///< Default discovery port for most platforms using IP sockets for communication.
#endif

#ifndef AkRegister
#define AkRegister
#endif
