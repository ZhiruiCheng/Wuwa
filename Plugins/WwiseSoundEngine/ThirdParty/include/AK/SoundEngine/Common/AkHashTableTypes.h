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

// Basic types for an open-addressed hash array (or, otherwise, keyState-value array).
// Exposing enough data and structures to used for forward declarations of types.
// Helper and inlineable functions, especially for use by IAkPluginServiceHashTable,
// are available in AkHashTableFuncs.h

// While the AkHashTableBase structures are exposed here, it is strongly recommended to use either the
// AkHashTable namespace functions to modify it, or the IAkPluginHashTable plug-in service, instead.

namespace AK
{
	// template instantiations available, in AkHashTable.cpp, are
	// template<AkUInt32>
	// template<AkUInt64> 
	template<typename KeyType>
	struct AkHashTableBase {
		AkHashTableBase()
			: pKeys(nullptr)
			, pbSlotOccupied(nullptr)
			, pValues(nullptr)
			, uNumReservedEntries(0)
			, uNumUsedEntries(0)
			, uMaxLoadFactor(kDefaultMaxLoadFactor)
		{
		}
		KeyType* pKeys;
		bool* pbSlotOccupied;
		void* pValues;

		AkUInt32 uNumReservedEntries;
		AkUInt32 uNumUsedEntries;
		AkUInt16 uValueElementSize;
		AkUInt16 uValueElementAlign;
		AkUInt32 uMaxLoadFactor;

		static const AkUInt32 kDefaultMaxLoadFactor = 28; // divided by 32 = 87% load. Robin-hood-style storage ensures that most probe lengths should be low even at this ratio
	};

	// main class to use; trivially convertible to AkHashTableBase
	// This allows for static disambiguation of hashtables for different value types; it's just syntactic sugar
	template<typename KeyType, typename ValueType>
	struct AkHashTable : public AkHashTableBase<KeyType>
	{
		KeyType KeyAt(AkUInt32 uSlot) const { return AkHashTableBase<KeyType>::pKeys[uSlot]; }

		ValueType* ValueData() { return (ValueType*)AkHashTableBase<KeyType>::pValues; }
		const ValueType* ValueData() const { return (const ValueType*)AkHashTableBase<KeyType>::pValues; }

		ValueType& ValueAt(AkUInt32 uSlot) { return ((ValueType*)AkHashTableBase<KeyType>::pValues)[uSlot]; }
		const ValueType& ValueAt(AkUInt32 uSlot) const { return ((ValueType*)AkHashTableBase<KeyType>::pValues)[uSlot]; }
	};

} // namespace AK
