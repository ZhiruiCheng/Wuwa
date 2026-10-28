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
/// Plug-in interface for accessing mixing-related functionality for plug-ins

#pragma once

#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/SoundEngine/Common/AkSpeakerConfig.h>
#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/IAkPlugin.h>

class AkMeterCtx;

namespace AK
{
	/// Interface for the "Meter" plug-in service, to handle metering of signals in various ways
	class IAkPluginServiceMeter : public IAkPluginService
	{
	protected:
		virtual ~IAkPluginServiceMeter() {}
	public:
		// Create an AkMeterCtx, to be used for later metering operations.
		// Because the AkMeterCtx object carries some state across each metering operation,
		// the same AkMeterCtx should be re-used for a given signal,
		// and should be used for only that signal
		virtual AkMeterCtx* InitMeter(
			AkChannelConfig in_channelCfg, // channel config of the audio signal to be metered
			AkMeteringFlags in_meterFlags // Flags indicating which metering operations should be active
		) = 0;

		// Destroys an AkMeterCtx and frees any associated memory with it.
		virtual void TermMeter(AkMeterCtx* io_pMeter) = 0;

		// Performs any metering necessary for the provided audio buffer, and returns the resulting AkMetering state
		virtual AK::AkMetering* MeterBuffer(AkMeterCtx* io_pMeter, AkAudioBuffer* in_pAudioBuffer, AkRamp in_gain) = 0;

		// Simple, stateless metering that does not require an AkMeterCtx 
		// in_ppfData should be an array of pointers to multiple channels of audio data, in_uNumChannels long, and each channel having in_uMaxFrames of data
		// Each channel's peak will be stored to the corresponding index in the out_pfChannelPeaks array. out_pfChannelPeaks must be in_uNumChannels long
		virtual void ComputePeaks(AkReal32* out_pfChannelPeaks, const AkReal32** in_ppfData, AkUInt32 in_uNumChannels, AkUInt32 in_uMaxFrames, AkRamp in_gain) = 0;

		// Simple, stateless metering that does not require an AkMeterCtx
		// Computes the mean-square value of the given audio data
		// (if performing this operation across multiple channels of a signal, sum up the results, then apply a square-root to obtain RMS)
		// in_ppfData should be an array of pointers to multiple channels of audio data, in_uNumChannels long, and each channel having in_uMaxFrames of data
		// Each channel's mean square will be stored to the corresponding index in the out_pfChannelMeanSquares array. out_pfChannelMeanSquares must be in_uNumChannels long
		virtual void ComputeMeanSquares(AkReal32* out_pfChannelMeanSquares, const AkReal32** in_ppfData, AkUInt32 in_uNumChannels, AkUInt32 in_uMaxFrames, AkRamp in_gain) = 0;

		// Fetches the previously-computed Metering state
		virtual AK::AkMetering* GetMetering(AkMeterCtx* io_pMeter) = 0;
	};
	
	#define AK_GET_PLUGIN_SERVICE_METER(plugin_ctx) static_cast<AK::IAkPluginServiceMeter*>(plugin_ctx->GetPluginService(AK::PluginServiceType_Meter))

}
