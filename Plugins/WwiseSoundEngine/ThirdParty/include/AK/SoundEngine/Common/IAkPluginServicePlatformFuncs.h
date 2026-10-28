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
#include <AK/SoundEngine/Common/IAkPlugin.h>

namespace AK
{
	/// Interface for the services related to the platform
	class IAkPluginServicePlatformFuncs : public IAkPluginService
	{
	protected:
		virtual ~IAkPluginServicePlatformFuncs() {}
	public:
		virtual void CreateThread(
			AkThreadRoutine in_pStartRoutine,              ///< Thread routine.
			void* in_pParams,                              ///< Routine parameters.
			const AkThreadProperties& in_threadProperties, ///< Properties. NULL for default.
			AkThread* out_pThread,                         ///< Returned thread handle.
			const char* in_szThreadName                    ///< Optional thread name.
		) = 0;
		virtual bool IsValidThread(AkThread* in_pThread) = 0;
		virtual void DestroyThread(AkThread* in_pThread) = 0;
		virtual void WaitForSingleThread(AkThread* in_pThread) = 0;
		virtual AkThreadID CurrentThread() = 0;
		virtual void GetDefaultThreadProperties(AkThreadProperties& out_threadProperties) = 0;
		virtual void GetDefaultHighPriorityThreadProperties(AkThreadProperties& out_threadProperties) = 0;
		virtual void Sleep(AkUInt32 in_ulMilliseconds) = 0;
	};

#define AK_GET_PLUGIN_SERVICE_PLATFORMFUNCS(plugin_ctx) static_cast<AK::IAkPluginServicePlatformFuncs*>(plugin_ctx->GetPluginService(AK::PluginServiceType_PlatformFuncs))

}
