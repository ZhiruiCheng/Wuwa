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

namespace AK {

 /// Coefficients to be used for application of digital biquad filters
struct AkBiquadCoefficients
{
	AkBiquadCoefficients()
		: fB0(0.f)
		, fB1(0.f)
		, fB2(0.f)
		, fA1(0.f)
		, fA2(0.f)
	{}
	AkBiquadCoefficients(AkReal32 _fB0, AkReal32 _fB1, AkReal32 _fB2, AkReal32 _fA1, AkReal32 _fA2)
		: fB0(_fB0)
		, fB1(_fB1)
		, fB2(_fB2)
		, fA1(_fA1)
		, fA2(_fA2)
	{}

	AkReal32 fB0;
	AkReal32 fB1;
	AkReal32 fB2;
	AkReal32 fA1;
	AkReal32 fA2;
};

/// "Memories" storing the previous state of the digital biquad filter
struct AkBiquadMemories
{
	AkReal32 fFFwd2;
	AkReal32 fFFwd1;
	AkReal32 fFFbk2;
	AkReal32 fFFbk1;

	AkBiquadMemories()
	{
		fFFwd2 = 0.f;
		fFFwd1 = 0.f;
		fFFbk2 = 0.f;
		fFFbk1 = 0.f;
	}
};

} // namespace
