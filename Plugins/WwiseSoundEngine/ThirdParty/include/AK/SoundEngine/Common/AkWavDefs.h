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

/// \file 
/// Basic definitions for WAV / WEM file parsers.

#include <AK/SoundEngine/Common/AkSpeakerConfig.h>
#include <AK/SoundEngine/Common/AkTypes.h>

#include <string.h> // for memcmp

//-----------------------------------------------------------------------------
// Constants.
//-----------------------------------------------------------------------------

// Standard IDs
constexpr AkFourcc RIFXChunkId = AkmmioFOURCC('R', 'I', 'F', 'X');
constexpr AkFourcc RIFFChunkId = AkmmioFOURCC('R', 'I', 'F', 'F');
constexpr AkFourcc RF64ChunkId = AkmmioFOURCC('R', 'F', '6', '4');
constexpr AkFourcc BW64ChunkId = AkmmioFOURCC('B', 'W', '6', '4');
constexpr AkFourcc ds64ChunkId = AkmmioFOURCC('d', 's', '6', '4');
constexpr AkFourcc WAVEChunkId = AkmmioFOURCC('W', 'A', 'V', 'E');
constexpr AkFourcc PLUGINChunkId = AkmmioFOURCC('P', 'L', 'U', 'G');
constexpr AkFourcc MIDIChunkId = AkmmioFOURCC('M', 'I', 'D', 'I');
constexpr AkFourcc fmtChunkId  = AkmmioFOURCC('f', 'm', 't', ' ');
constexpr AkFourcc dataChunkId = AkmmioFOURCC('d', 'a', 't', 'a');
constexpr AkFourcc cueChunkId  = AkmmioFOURCC('c', 'u', 'e', ' ');
constexpr AkFourcc LISTChunkId = AkmmioFOURCC('L', 'I', 'S', 'T');
constexpr AkFourcc adtlChunkId = AkmmioFOURCC('a', 'd', 't', 'l');
constexpr AkFourcc lablChunkId = AkmmioFOURCC('l', 'a', 'b', 'l');
constexpr AkFourcc smplChunkId = AkmmioFOURCC('s', 'm', 'p', 'l');
constexpr AkFourcc junkChunkId = AkmmioFOURCC('j', 'u', 'n', 'k');
constexpr AkFourcc axmlChunkId = AkmmioFOURCC('a', 'x', 'm', 'l');
constexpr AkFourcc chnaChunkId = AkmmioFOURCC('c', 'h', 'n', 'a');
constexpr AkFourcc iXMLChunkId = AkmmioFOURCC('i', 'X', 'M', 'L');

constexpr AkUInt16 extensibleWavFormat = 0xFFFE;
constexpr AkUInt16 floatWavFormat = 0x0003;
constexpr AkUInt16 pcmWavFormat = 0x0001;

constexpr AkUInt32 kWavHashSize = 16;

// AK-specific IDs
constexpr AkFourcc akdChunkId  = AkmmioFOURCC('a', 'k', 'd', ' ');
constexpr AkFourcc hashChunkId = AkmmioFOURCC('h', 'a', 's', 'h');
constexpr AkFourcc seekChunkId = AkmmioFOURCC('s', 'e', 'e', 'k');
constexpr AkFourcc irmdChunkId = AkmmioFOURCC('i', 'r', 'm', 'd');
constexpr AkFourcc akdmChunkId = AkmmioFOURCC('a', 'k', 'd', 'm');

//-----------------------------------------------------------------------------
// Structs.
//-----------------------------------------------------------------------------

#pragma pack(push, 1)

/// Standard WAV chunk header
struct AkChunkHeader
{
	AkFourcc	fccChunkId;
	AkUInt32	dwDataSize;
};

struct ChunkSize64
{
	AkFourcc fccChunkId;
	AkUInt32 dwDataSizeLow;
	AkUInt32 dwDataSizeHigh;
};

struct DataSize64
{
	AkUInt32 dwRiffSizeLow;
	AkUInt32 dwRiffSizeHigh;
	AkUInt32 dwDataSizeLow;
	AkUInt32 dwDataSizeHigh;
	AkUInt32 dwSampleCountLow;
	AkUInt32 dwSampleCountHigh;
	AkUInt32 dwTableLength; // number of table[] entries
	// ChunkSize64 table[];
};

// General waveform format structure (information common to all formats)
struct WaveFormat
{
	AkUInt16    wFormatTag;        // format type
	AkUInt16    nChannels;         // number of channels (i.e. mono, stereo...)
	AkUInt32	nSamplesPerSec;    // sample rate
	AkUInt32	nAvgBytesPerSec;   // for buffer estimation
	AkUInt16	nBlockAlign;       // block size of data
};

// Specific waveform format structure for PCM data
struct PcmWaveFormat : WaveFormat
{
	AkUInt16	wBitsPerSample;
};

// General extended waveform format structure
struct WaveFormatEx : PcmWaveFormat
{
	AkUInt16	cbSize;	// The count in bytes of the size of extra information (after cbSize)
};
static_assert(sizeof(WaveFormatEx) == 18, "Incorrect padding for WaveFormatEx");

struct WaveFormatExtensible : public WaveFormatEx
{
	union {
		AkUInt16 wValidBitsPerSample;	// bits of precision
		AkUInt16 wSamplesPerBlock;      // valid if wBitsPerSample == 0
		AkUInt16 wReserved;             // If neither applies, set to zero.
	} Samples;

	AkUInt32    dwChannelMask;			// which channels are present in stream

	struct GUID final
	{
		AkUInt32 Data1;
		AkUInt16 Data2;
		AkUInt16 Data3;
		AkUInt8	 Data4[8];

		inline bool operator==(const GUID& rhs)
		{
			return memcmp(this, &rhs, sizeof(GUID)) == 0;
		}

		inline bool operator!=(const GUID& rhs)
		{
			return !(*this == rhs);
		}
	} SubFormat;
};
static_assert(sizeof(WaveFormatExtensible) == sizeof(WaveFormatEx) + 6 + 16, "Incorrect padding for WaveFormatExtensible");

static constexpr WaveFormatExtensible::GUID G_KSDATAFORMAT_SUBTYPE_PCM = { 0x0001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static constexpr WaveFormatExtensible::GUID G_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = { 0x0003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

/// WEM format header: equivalent to WaveFormatExtensible, with an AkChannelConfig
/// instead of dwChannelMask+SubFormat. Codecs that require format-specific chunks 
/// should extend this structure.
struct WemFormatExtensible : public WaveFormatEx
{
	AkUInt16    wSamplesPerBlock;
	AkUInt32    uChannelConfig;		// Serialized AkChannelConfig

	inline AkChannelConfig GetChannelConfig() const
	{
		AkChannelConfig channelConfig;
		channelConfig.Deserialize(uChannelConfig);
		return channelConfig;
	}
};
static_assert(sizeof(WemFormatExtensible) == 24, "Incorrect padding for WemFormatExtensible");

struct SamplerChunk
{
	AkUInt32     dwManufacturer;
	AkUInt32     dwProduct;
	AkUInt32     dwSamplePeriod;
	AkUInt32     dwMIDIUnityNote;
	AkUInt32     dwMIDIPitchFraction;
	AkUInt32     dwSMPTEFormat;
	AkUInt32     dwSMPTEOffset;
	AkUInt32     dwSampleLoops;
	AkUInt32     dwSamplerData;
};

struct SampleLoop
{
	AkUInt32     dwIdentifier;
	AkUInt32     dwType;
	AkUInt32     dwStart;
	AkUInt32     dwEnd;
	AkUInt32     dwFraction;
	AkUInt32     dwPlayCount;
};

//-----------------------------------------------------------------------------
// Name: struct WaveCue
// Desc: Cue points point contained in CUE chunk
//-----------------------------------------------------------------------------
struct WaveCue
{
	friend bool operator<(const WaveCue& in_rLeft, const WaveCue& in_rRight)
	{
		return in_rLeft.dwPosition < in_rRight.dwPosition;
	}

	AkUInt32 dwCueId;
	AkUInt32 dwPosition;
	AkUInt32 dwRiffId;
	AkUInt32 dwChunkStart;
	AkUInt32 dwBlockStart;
	AkUInt32 dwSampleOffset;
};

struct ChnaAudioId
{
	AkUInt16    trackIndex;     // index of track in file 
	char		UID[12];        // audioTrackUID value 
	char		trackRef[14];   // audioTrackFormatID reference 
	char		packRef[11];    // audioPackFormatID reference 
	char		pad;            // padding byte to ensure even number of bytes 
};

struct ChnaChunk
{
	AkUInt16    numTracks;      // number of tracks used 
	AkUInt16    numUIDs;        // number of track UIDs used 
	// ChnaAudioId ID[0];     // IDs for each track  (where N >= numUIDs) 
};

#pragma pack(pop)

namespace AkWAVStructs
{
	struct AkWaveFileHeader
	{
		AkChunkHeader	riffHdr;
		AkFourcc		waveId;
	};

	struct AkSamplerOneLoop
	{
		SamplerChunk sampler;
		SampleLoop loop;
	};

	struct AkWaveMarker
	{
		uint32_t dwCueId;
		uint32_t dwPosition;	// in audio frames
		uint32_t dwLabelSize;	// character count in pLabel including any potential null terminator
		char* pLabel;			// NOT null-terminated: pascal-style array of dwLabelSize characters
	};

	struct IrMetadata
	{
		float	fPeakAmp;
		float	fRT60;
	};

	enum ChunkType
	{
		ChunkType_FMT = 1 << 0,
		ChunkType_DATA = 1 << 1,
		ChunkType_SMPL = 1 << 2,
		ChunkType_CUE = 1 << 3,
		ChunkType_LIST = 1 << 4,
		ChunkType_IRMD = 1 << 5,
		// ChunkType_Reserved = 1<<6, // See AkConvolutionReverbTypes.h
		ChunkType_HASH = 1 << 7,
		ChunkType_JUNK = 1 << 8,
		ChunkType_CHNA = 1 << 9,
		ChunkType_AXML = 1 << 10,
		ChunkType_AKDM = 1 << 11,
		ChunkType_IXML = 1 << 12,

		ChunkType_Required = ChunkType_FMT | ChunkType_DATA,
		ChunkType_ALL = 0xffffffff
	};
}

namespace AK
{
	inline AkChannelConfig SetChannelConfigFromWaveFormatExtensible(const WaveFormatExtensible& in_wfmext)
	{
		AkChannelConfig channelConfig;
		if (in_wfmext.wFormatTag != extensibleWavFormat)
		{
			// For backward compatibility, if the number of channels is smaller or equal to 8, we interpret it as 
			// a standard multichannel file.
			channelConfig.SetStandardOrAnonymous(in_wfmext.nChannels, AK::ChannelMaskFromNumChannels(in_wfmext.nChannels));
		}
		else
		{
			if (in_wfmext.dwChannelMask == 0)
			{
				// Ambisonic?
				static const WaveFormatExtensible::GUID s_ambisonic = { 0x00000001, 0x0721, 0x11D3, { 0x86, 0x44, 0xC8, 0xC1, 0xCA, 0x00, 0x00, 0x00 } };
				// AMB GUID must match and config must be supported.
				if (memcmp(&in_wfmext.SubFormat, &s_ambisonic, sizeof(WaveFormatExtensible::GUID)) == 0
					&& in_wfmext.nChannels != 10
					&& (in_wfmext.nChannels < 12
						|| in_wfmext.nChannels == 16
						|| in_wfmext.nChannels == 25
						|| in_wfmext.nChannels == 36))
				{
					channelConfig.SetAmbisonic(in_wfmext.nChannels);
				}
				else
				{
					// WG-18148: Pro-Tools can generate extensibleWavFormat with channelmask==0
					channelConfig.SetStandardOrAnonymous(in_wfmext.nChannels, AK::ChannelMaskFromNumChannels(in_wfmext.nChannels));
				}
			}
			else
			{
				AkUInt32 uChannelMask = in_wfmext.dwChannelMask & AK_SPEAKER_SETUP_ALL_SPEAKERS;
				// Verify consistency of wave extensible descriptor.
				if (in_wfmext.nChannels == ChannelMaskToNumChannels(uChannelMask))
				{
					AK_SPEAKER_SETUP_CONVERT_TO_SUPPORTED(uChannelMask);
					channelConfig.SetStandard(uChannelMask);
				}
				else
				{
					// Mismatch between mask and nChannels; trust nChannels for backward compatibility.
					channelConfig.SetStandardOrAnonymous(in_wfmext.nChannels, AK::ChannelMaskFromNumChannels(in_wfmext.nChannels));
				}
			}
		}
		return channelConfig;
	}
}
