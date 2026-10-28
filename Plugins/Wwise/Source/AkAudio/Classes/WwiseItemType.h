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

#include "WwiseDefines.h"

#include "Containers/UnrealString.h"
#include "Engine/EngineTypes.h"

/**
 * @enum EWwiseItemType
 * @brief Enumeration representing different types of Wwise items in a Wwise project.
 *
 * This enum defines all the possible item types that can be found in the Wwise project hierarchy,
 * including events, busses, switches, states, and various container types.
 */
UENUM()
enum class EWwiseItemType
{
	Event,
	AuxBus,
	Switch,
	State,
	GameParameter,
	DialogueEvent,
	EffectShareSet,
	Trigger,
	AcousticTexture,
	AudioDeviceShareSet,
	ActorMixer,
	Bus, 
	Project,
	StandaloneWorkUnit,
	NestedWorkUnit,
	PhysicalFolder,
	Folder,
	Sound,
	SwitchContainer,
	RandomSequenceContainer,
	BlendContainer,
	MotionBus,
	StateGroup,
	SwitchGroup,
	InitBank,
	AudioNode,

	First = Event,
	Last = AudioNode,
	LastWwiseBrowserType = AudioDeviceShareSet,

	None = -1,
};

/**
 * @brief Pre-increment operator for EWwiseItemType enumeration.
 *
 * Advances the enum value to the next item type. Used for iterating through item types.
 *
 * @param Item Reference to the EWwiseItemType value to increment.
 * @return Reference to the incremented EWwiseItemType value.
 * @note Asserts if incrementing beyond EWwiseItemType::Last.
 */
inline EWwiseItemType& operator++(EWwiseItemType& Item)
{
	using Underlying = std::underlying_type_t<EWwiseItemType>;
	auto Result = static_cast<Underlying>(Item) + 1;
	check(Result <= static_cast<Underlying>(EWwiseItemType::Last));
	Item = static_cast<EWwiseItemType>(Result);
	return Item;
}

/**
 * @brief Post-increment operator for EWwiseItemType enumeration.
 *
 * Advances the enum value to the next item type, returning the original value.
 * Used for iterating through item types.
 *
 * @param Item Reference to the EWwiseItemType value to increment.
 * @return The original EWwiseItemType value before incrementing.
 * @note Asserts if incrementing beyond EWwiseItemType::Last.
 */
inline EWwiseItemType operator++(EWwiseItemType& Item, int)
{
	auto Orig = Item;
	using Underlying = std::underlying_type_t<EWwiseItemType>;
	auto Result = static_cast<Underlying>(Item) + 1;
	check(Result <= static_cast<Underlying>(EWwiseItemType::Last));
	Item = static_cast<EWwiseItemType>(Result);
	return Orig;
}

/**
 * @namespace WwiseItemType
 * @brief Contains display names, folder names, and utility functions for Wwise item types.
 *
 * This namespace provides constants and helper functions for working with Wwise item types,
 * including human-readable display names for the browser UI and folder names that correspond
 * to the Wwise project structure on disk.
 */
namespace WwiseItemType
{
	static const FString EventsBrowserName = TEXT("Events");
	static const FString DialogueEventsBrowserName = TEXT("Dynamic Dialogue");
	static const FString BussesBrowserName = TEXT("Busses");
	static const FString AcousticTexturesBrowserName = TEXT("Virtual Acoustics");
	static const FString AudioDeviceShareSetBrowserName = TEXT("Device ShareSets");
	static const FString StatesBrowserName = TEXT("States");
	static const FString SwitchesBrowserName = TEXT("Switches");
	static const FString GameParametersBrowserName = TEXT("Game Parameters");
	static const FString TriggersBrowserName = TEXT("Triggers");
	static const FString ShareSetsBrowserName =	TEXT("Effect ShareSets");
	static const FString OrphanAssetsBrowserName = TEXT("Orphan Assets");

	/**
	 * @brief Display names shown in the Wwise Browser UI.
	 *
	 * This array contains human-readable names for each category of Wwise items
	 * as they appear in the browser interface. The order corresponds to the
	 * browser tab ordering.
	 *
	 * @see EWwiseItemType
	 */
	static const FString BrowserDisplayNames[] = {
		EventsBrowserName,
		DialogueEventsBrowserName,
		BussesBrowserName,
		AcousticTexturesBrowserName,
		AudioDeviceShareSetBrowserName,
		StatesBrowserName,
		SwitchesBrowserName,
		GameParametersBrowserName,
		TriggersBrowserName,
		ShareSetsBrowserName,
		OrphanAssetsBrowserName
	};

	/**
	 * @brief Folder names containing Work Units for each Wwise item type.
	 *
	 * This array maps to the actual folder names in the Wwise project directory
	 * structure where Work Units of each type are stored. These names may vary
	 * between Wwise versions.
	 *
	 * @note Folder names differ between Wwise versions (pre-2025.1 vs 2025.1+).
	 * @see EWwiseItemType
	 */
	static const FString FolderNames[] = {
		TEXT("Events"),
#if WWISE_2025_1_OR_LATER
		TEXT("Busses"),
#else
		TEXT("Master-Mixer Hierarchy"),
#endif
		TEXT("Switches"),
		TEXT("States"),
		TEXT("Game Parameters"),
		TEXT("Dynamic Dialogue"),
		TEXT("Effects"),
		TEXT("Triggers"),
		TEXT("Virtual Acoustics"),
#if WWISE_2025_1_OR_LATER
		TEXT("Devices")
#else
		TEXT("Audio Devices")
#endif
	};

	/**
	 * @brief Converts a string representation to its corresponding EWwiseItemType value.
	 *
	 * Parses the given item name string and returns the matching enumeration value.
	 * This is useful for converting Wwise project type names to the internal enum representation.
	 *
	 * @param ItemName The string name of the Wwise item type (e.g., "Event", "AuxBus", "Sound").
	 * @return The corresponding EWwiseItemType value, or EWwiseItemType::None if not found.
	 *
	 * @code
	 * EWwiseItemType Type = WwiseItemType::FromString(TEXT("Event"));
	 * // Type == EWwiseItemType::Event
	 *
	 * EWwiseItemType InvalidType = WwiseItemType::FromString(TEXT("Unknown"));
	 * // InvalidType == EWwiseItemType::None
	 * @endcode
	 */
	inline EWwiseItemType FromString(const FString& ItemName)
	{
		struct TypePair
		{
			FString Name;
			EWwiseItemType Value;
		};

		static const TypePair ValidTypes[] = {
			{TEXT("AcousticTexture"), EWwiseItemType::AcousticTexture},
			{TEXT("AudioDevice"), EWwiseItemType::AudioDeviceShareSet},
			{TEXT("ActorMixer"), EWwiseItemType::ActorMixer},
			{TEXT("AuxBus"), EWwiseItemType::AuxBus},
			{TEXT("BlendContainer"), EWwiseItemType::BlendContainer},
			{TEXT("Bus"), EWwiseItemType::Bus},
			{TEXT("DialogueEvent"), EWwiseItemType::DialogueEvent},
			{TEXT("Event"), EWwiseItemType::Event},
			{TEXT("Folder"), EWwiseItemType::Folder},
			{TEXT("GameParameter"), EWwiseItemType::GameParameter},
			{TEXT("MotionBus"), EWwiseItemType::MotionBus},
			{TEXT("PhysicalFolder"), EWwiseItemType::PhysicalFolder},
			{TEXT("Project"), EWwiseItemType::Project},
			{TEXT("RandomSequenceContainer"), EWwiseItemType::RandomSequenceContainer},
			{TEXT("Sound"), EWwiseItemType::Sound},
			{TEXT("State"), EWwiseItemType::State},
			{TEXT("StateGroup"), EWwiseItemType::StateGroup},
			{TEXT("Switch"), EWwiseItemType::Switch},
			{TEXT("SwitchContainer"), EWwiseItemType::SwitchContainer},
			{TEXT("SwitchGroup"), EWwiseItemType::SwitchGroup},
			{TEXT("Trigger"), EWwiseItemType::Trigger},
			{TEXT("WorkUnit"), EWwiseItemType::StandaloneWorkUnit},
			{TEXT("Effect"), EWwiseItemType::EffectShareSet},
		};

		for (const auto& type : ValidTypes)
		{
			if (type.Name == ItemName)
			{
				return type.Value;
			}
		}

		return EWwiseItemType::None;
	}
};
