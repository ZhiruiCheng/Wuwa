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

#include <AK/SoundEngine/Common/AkTypedefs.h>

static const AkPluginID               AK_INVALID_PLUGINID = (AkPluginID)-1;                ///< Invalid FX ID
static const AkPluginID               AK_INVALID_SHARE_SET_ID = (AkPluginID)-1;            ///< Invalid Share Set ID
static const AkGameObjectID           AK_INVALID_GAME_OBJECT = (AkGameObjectID)-1;         ///< Invalid game object (may also mean all game objects)
static const AkUniqueID               AK_INVALID_UNIQUE_ID = 0;                            ///< Invalid unique 32-bit ID
static const AkRtpcID                 AK_INVALID_RTPC_ID = 0;                              ///< Invalid RTPC ID
static const AkPlayingID              AK_INVALID_PLAYING_ID = 0;                           ///< Invalid playing ID
static const AkUInt32                 AK_DEFAULT_SWITCH_STATE = 0;                         ///< Switch selected if no switch has been set yet
static const AkMemPoolId              AK_INVALID_POOL_ID = -1;                             ///< Invalid pool ID
static const AkMemPoolId              AK_DEFAULT_POOL_ID = -1;                             ///< Default pool ID, same as AK_INVALID_POOL_ID
static const AkAuxBusID               AK_INVALID_AUX_ID = 0;                               ///< Invalid auxiliary bus ID (or no Aux bus ID)
static const AkFileID                 AK_INVALID_FILE_ID = (AkFileID)-1;                   ///< Invalid file ID
static const AkCacheID                AK_INVALID_CACHE_ID = (AkCacheID)-1;                 ///< Invalid cache ID
static const AkDeviceID               AK_INVALID_DEVICE_ID = (AkDeviceID)-1;               ///< Invalid streaming device ID
static const AkBankID                 AK_INVALID_BANK_ID = 0;                              ///< Invalid bank ID
static const AkArgumentValueID        AK_FALLBACK_ARGUMENTVALUE_ID = 0;                    ///< Fallback argument value ID
static const AkChannelMask            AK_INVALID_CHANNELMASK = 0;                          ///< Invalid channel mask
static const AkUInt32                 AK_INVALID_OUTPUT_DEVICE_ID = 0;                     ///< Invalid Device ID
static const AkPipelineID             AK_INVALID_PIPELINE_ID = 0;                          ///< Invalid pipeline ID (for profiling)
static const AkAudioObjectID          AK_INVALID_AUDIO_OBJECT_ID = (AkAudioObjectID)-1;    ///< Invalid audio object ID

static const AkGameObjectID           AK_TRANSPORT_GAME_OBJECT = (AkGameObjectID)(-2);     ///< Game object ID used by Wwise Authoring when playing back sounds using the Transport view.
static const AkGameObjectID           AK_DIRECT_GAME_OBJECT = (AkGameObjectID)(-3);        ///< Game object ID used by Wwise Authoring when playing back sounds directly using the transport or media pool.

// Priority.
static const AkPriority               AK_DEFAULT_PRIORITY = 50;               ///< Default sound / I/O priority
static const AkPriority               AK_MIN_PRIORITY = 0;                    ///< Minimal priority value [0,100]
static const AkPriority               AK_MAX_PRIORITY = 100;                  ///< Maximal priority value [0,100]

// Default bank I/O settings.
static const AkPriority               AK_DEFAULT_BANK_IO_PRIORITY = 50;                       ///<  Default bank load I/O priority
static const AkReal32                 AK_DEFAULT_BANK_THROUGHPUT = 1 * 1024 * 1024 / 1000.f;  ///<  Default bank load throughput (1 Mb/ms)

// Bank version
static const AkUInt32                 AK_SOUNDBANK_VERSION = 172;     ///<  Version of the soundbank reader (update AkBanks.h as well)

// Job types
static const AkJobType                AkJobType_Generic = 0;              ///< Job type for general-purpose work
static const AkJobType                AkJobType_AudioProcessing = 1;      ///< Job type for DSP work
static const AkJobType                AkJobType_SpatialAudio = 2;         ///< Job type for Spatial Audio computations
static const AkUInt32                 AK_NUM_JOB_TYPES = 3;               ///< Number of possible job types recognized by the Sound Engine

// ---------------------------------------------------------------
// Languages
// ---------------------------------------------------------------
#define AK_MAX_LANGUAGE_NAME_SIZE    (32)

// ---------------------------------------------------------------
// File Type ID Definitions
// ---------------------------------------------------------------

// These correspond to IDs specified in the conversion plug-ins' XML
// files. Audio sources persist them to "remember" their format.
// DO NOT CHANGE THEM without talking to someone in charge of persistence!

// Company ID for plugin development.
#define AKCOMPANYID_PLUGINDEV_MIN         (64)
#define AKCOMPANYID_PLUGINDEV_MAX         (255)

// Vendor ID.
#define AKCOMPANYID_AUDIOKINETIC          (0)      ///< Audiokinetic inc.
#define AKCOMPANYID_AUDIOKINETIC_EXTERNAL (1)      ///< Audiokinetic inc.
#define AKCOMPANYID_MCDSP                 (256)    ///< McDSP
#define AKCOMPANYID_WAVEARTS              (257)    ///< WaveArts
#define AKCOMPANYID_PHONETICARTS          (258)    ///< Phonetic Arts
#define AKCOMPANYID_IZOTOPE               (259)    ///< iZotope
#define AKCOMPANYID_CRANKCASEAUDIO        (261)    ///< Crankcase Audio
#define AKCOMPANYID_IOSONO                (262)    ///< IOSONO
#define AKCOMPANYID_AUROTECHNOLOGIES      (263)    ///< Auro Technologies
#define AKCOMPANYID_DOLBY                 (264)    ///< Dolby
#define AKCOMPANYID_TWOBIGEARS            (265)    ///< Two Big Ears
#define AKCOMPANYID_OCULUS                (266)    ///< Oculus
#define AKCOMPANYID_BLUERIPPLESOUND       (267)    ///< Blue Ripple Sound
#define AKCOMPANYID_ENZIEN                (268)    ///< Enzien Audio
#define AKCOMPANYID_KROTOS                (269)    ///< Krotos (Dehumanizer)
#define AKCOMPANYID_NURULIZE              (270)    ///< Nurulize
#define AKCOMPANYID_SUPERPOWERED          (271)    ///< Super Powered
#define AKCOMPANYID_GOOGLE                (272)    ///< Google
//#define AKCOMPANYID_NVIDIA              (273)    ///< NVIDIA        // Commented out to avoid redefinition, provider is already defining it.
//#define AKCOMPANYID_RESERVED            (274)    ///< Reserved    // Commented out to avoid redefinition, provider is already defining it.
//#define AKCOMPANYID_MICROSOFT           (275)    ///< Microsoft    // Commented out to avoid redefinition, provider is already defining it.
//#define AKCOMPANYID_YAMAHA              (276)    ///< YAMAHA        // Commented out to avoid redefinition, provider is already defining it.
#define AKCOMPANYID_VISISONICS            (277)    ///< Visisonics

// File/encoding types of Audiokinetic.
#define AKCODECID_BANK                  (0)     ///< Bank encoding
#define AKCODECID_PCM                   (1)     ///< PCM encoding
#define AKCODECID_ADPCM                 (2)     ///< ADPCM encoding
#define AKCODECID_XMA                   (3)     ///< XMA encoding
#define AKCODECID_VORBIS                (4)     ///< Vorbis encoding
#define AKCODECID_WIIADPCM              (5)     ///< ADPCM encoding on the Wii
#define AKCODECID_PCM_WAV               (7)     ///< Standard PCM WAV file parser
#define AKCODECID_EXTERNAL_SOURCE       (8)     ///< External Source (unknown encoding)
#define AKCODECID_XWMA                  (9)     ///< xWMA encoding
#define AKCODECID_FILE_PACKAGE          (11)    ///< File package files generated by the File Packager utility.
#define AKCODECID_ATRAC9                (12)    ///< ATRAC-9 encoding
#define AKCODECID_VAG                   (13)    ///< VAG/HE-VAG encoding
#define AKCODECID_PROFILERCAPTURE       (14)    ///< Profiler capture file (.prof) as written through AK::SoundEngine::StartProfilerCapture
#define AKCODECID_ANALYSISFILE          (15)    ///< Analysis file
#define AKCODECID_MIDI                  (16)    ///< MIDI file
#define AKCODECID_OPUSNX                (17)    ///< OpusNX encoding
#define AKCODECID_CAF                   (18)    ///< CAF file
#define AKCODECID_AKOPUS                (19)    ///< Opus encoding, 2018.1 to 2019.2
#define AKCODECID_AKOPUS_WEM            (20)    ///< Opus encoding, wrapped in WEM
#define AKCODECID_MEMORYMGR_DUMP        (21)    ///< Memory stats file as written through AK::MemoryMgr::DumpToFile();
#define AKCODECID_SONY360               (22)    ///< Sony 360 encoding

#define AKCODECID_BANK_EVENT            (30)    ///< Bank encoding for event banks. These banks are contained in the /event sub-folder.
#define AKCODECID_BANK_BUS              (31)    ///< Bank encoding for bus banks. These banks are contained in the /bus sub-folder.
         
#define AKPLUGINID_RECORDER                      (132)   ///< Recorder Plugin
#define AKPLUGINID_IMPACTER                      (184)   ///< Impacter Plugin
#define AKPLUGINID_SYSTEM_OUTPUT_META            (900)   ///< System output metadata
#define AKPLUGINID_AUDIO_OBJECT_ATTENUATION_META (901)   ///< Attenuation curve metadata
#define AKPLUGINID_AUDIO_OBJECT_PRIORITY_META    (902)   ///< Audio object priority metadata

#define AKEXTENSIONID_SPATIALAUDIO        (800)    ///< Spatial Audio
#define AKEXTENSIONID_INTERACTIVEMUSIC    (801)    ///< Interactive Music
#define AKEXTENSIONID_MIDIDEVICEMGR       (802)    ///< MIDI Device Manager (Authoring)

//The following are internally defined
#define AK_WAVE_FORMAT_VAG              0xFFFB
#define AK_WAVE_FORMAT_AT9              0xFFFC
#define AK_WAVE_FORMAT_VORBIS           0xFFFF
#define AK_WAVE_FORMAT_OPUSNX           0x3039
#define AK_WAVE_FORMAT_OPUS             0x3040
#define AK_WAVE_FORMAT_OPUS_WEM         0x3041
#define AK_WAVE_FORMAT_XMA2             0x166

#define AK_INVALID_SAMPLE_POS 0xFFFFFFFF
