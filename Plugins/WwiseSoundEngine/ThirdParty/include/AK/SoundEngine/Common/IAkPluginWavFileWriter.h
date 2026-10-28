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

/// \file
/// Plug-in interface for wav file writing

#pragma once

#include <AK/SoundEngine/Common/IAkPlugin.h>

namespace AK
{
	/// Interface for the wav file writer.
	class IAkPluginWavFileWriter
	{
	public:
		/// Pass audio data to be written as-is into the data chunk of the wav file.
		virtual AKRESULT PassAudioData(
			void* in_pData,  ///< Pointer to audio data
			AkUInt32 in_size ///< Size of audio data
			) = 0;

		/// Pass extra data to be written as-is after the data chunk of the wav file,
		/// including any associated wav chunk header.
		virtual AKRESULT PassExtraData(
			void* in_pData,  ///< Pointer to extra data
			AkUInt32 in_size ///< Size of extra data
		) = 0;

		/// Destroy the writer, closing the file and freeing all resources.
		virtual void Destroy() = 0;
	};

	/// Interface for the wav file writer service.
	class IAkPluginServiceWavFileWriter
		: public IAkPluginService
	{
	protected:
		virtual ~IAkPluginServiceWavFileWriter() {}
	public:
		enum FormatType { WAV = 0, WEM = 1, ADM = 2 };

		/// Start writing into a new wav file. Upon success, a writer is returned via out_ppWriter.
		virtual AKRESULT Start(
			AkOSChar* in_pszPath,                  ///< Path to wav file
			AkChannelConfig in_channelConfig,      ///< Channel configuration of wav file
			AkUInt32 in_uSampleRate,               ///< Sample rate of wav file
			FormatType in_eFormat,                 ///< Format of wav file
			uint64_t in_sourceHash[2],             ///< Optional hash value to be written into a hash chunk
			IAkPluginWavFileWriter** out_ppWriter  ///< Returned writer interface
			) = 0;
	};

} // namespace AK

#define AK_GET_PLUGIN_SERVICE_WAV_FILE_WRITER(plugin_ctx) static_cast<AK::IAkPluginServiceWavFileWriter*>(plugin_ctx->GetPluginService(AK::PluginServiceType_WavFileWriter))
