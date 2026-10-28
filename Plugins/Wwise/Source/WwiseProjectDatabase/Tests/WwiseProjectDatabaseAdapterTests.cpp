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

#ifdef TESTING_WWISE_PROJECT_DATABASE
#include <catch2/catch.hpp>
#include "AdapterTypes/WwiseWrapperTypes.h"
#else
#include "Wwise/AdapterTypes/WwiseWrapperTypes.h"
#endif
#include "Wwise/AdapterTypes/Unreal/WwiseProjectDatabaseTestingUtils.h"

SCENARIO("Wwise::WwiseProjectDatabase::AdapterTypes")
{
	SECTION("Guid")
	{
		//Guid String Constructor
		WwiseDBGuid Guid(WwiseDBString("{ABCDDCBA-1234-4321-1357-BA9876543210}"));
		WWISE_CHECK(Guid.ToString() == WwiseDBString("{ABCDDCBA-1234-4321-1357-BA9876543210}"));

		//Guid String Constructor with bad format
		WwiseDBGuid BadGuid(WwiseDBString("I am GUID"));
		WWISE_CHECK(!BadGuid.IsValid());

		//Guid String Constructor with good format but bad hexadecimal format
		BadGuid = WwiseDBGuid(WwiseDBString("{ABCDDCBA-1234-4321-1357-BH9876543210}"));
		WWISE_CHECK(!BadGuid.IsValid());

		//Guid Constructed with Strings return the correct values
		int A, B, C, D;
		Guid.GetGuidValues(A, B, C, D);
		WWISE_CHECK(A == -1412571974);
		WWISE_CHECK(B == 305414945);
		WWISE_CHECK(C == 324516504);
		WWISE_CHECK(D == 1985229328);
		WWISE_CHECK(Guid.IsValid());

		//Guid Constructor using int
		WwiseDBGuid NumberGuid(A, B, C, D);
		WWISE_CHECK(NumberGuid.ToString() == WwiseDBString("{ABCDDCBA-1234-4321-1357-BA9876543210}"));

		//Checking Guids with at least one 0 but not all 4 is valid. 
		NumberGuid = WwiseDBGuid(0, 1, 2, 3);
		WWISE_CHECK(NumberGuid.IsValid());

		//Checking Guids with only 0 is not valid.
		NumberGuid = WwiseDBGuid(0, 0, 0, 0);
		WWISE_CHECK(!NumberGuid.IsValid());

		//Checking Guids with multiple 0 at the start
		Guid = WwiseDBGuid(WwiseDBString("{00C0DC0A-1234-4321-1357-BA9876543210}"));
		WWISE_CHECK(Guid.ToString() == WwiseDBString("{00C0DC0A-1234-4321-1357-BA9876543210}"));
	}

	SECTION("ParallelFor")
	{
		WwiseDBArray<int> Values;
		WwiseDBArray<int> ValuesParallel;
		ValuesParallel.Reserve(1000);
		
		//Setting values Asynchronously in an array.
		for(int i = 0; i < 1000; i++)
		{
			Values.Add(i);
			ValuesParallel.Add(i);
		}
		WwiseDBAsync::WwiseParallelFor(
			Values,
			[&ValuesParallel, &Values](const auto& n)
		    {
				ValuesParallel[Values(n)] = Values(n);
			}
		);

		//Doing modifications Asynchronously
		WwiseDBAsync::WwiseParallelFor(
			Values,
			[&ValuesParallel, &Values](const auto& n)
			{
				Values[Values(n)] = Values(n) + 1;
			}
		);

		for(int i = 0; i < 1000; i++)
		{
			WWISE_CHECK(ValuesParallel[i] == i);
			WWISE_CHECK(Values[i] == i + 1);
		}

		WwiseDBMap<int, int> MapValues;
		WwiseDBMap<int, int> MapValuesParallel;
		//Setting values Asynchronously in a map.
		for(int i = 0; i < 1000; i++)
		{
			MapValues.Add(i, i);
			MapValuesParallel.Add(i, 0);
		}
		WwiseDBAsync::WwiseParallelFor(
			MapValues,
			[&MapValuesParallel, &MapValues](const auto& n)
			{
				WwiseDBPair<int, int> Pair(MapValues(n));
				MapValuesParallel.FindChecked(Pair.GetFirst()) = Pair.GetSecond();
			}
		);
		
		for(int i = 0; i < 1000; i++)
		{
			WWISE_CHECK(*MapValuesParallel.Find(i) == i);
		}
	}

	SECTION("Map")
	{
		WwiseDBMap<int, int> Map;
		//Adding a Key to a Map
		Map.Add(1, 1);
		WWISE_CHECK(Map[1] == 1);
		
		//Adding an already existing Key to a Map. This will not replace the value.
		Map.Add(1, 2);
		WWISE_CHECK(Map[1] == 1);

		//Adding or replacing an already existing Key to a Map. This will replace the value.
		Map.AddOrReplace(1, 2);
		WWISE_CHECK(Map[1] == 2);

		//Searching for a key that does not exist. Returns nullptr.
		WWISE_CHECK(!Map.Find(2));

		//Finding a value from a key and editing it.
		Map.FindChecked(1) = 3;
		WWISE_CHECK(Map[1] == 3);

		//Emptying the Map.
		Map.Empty();
		WWISE_CHECK(Map.Size() == 0);

		//Searching for a key that does not exist. No Crash
		Map[1];

		Map.Add(1, 2);
		Map.Add(2, 2);
		int count = 0;
		//Iterator Test. It passes through the whole map.
		for(auto& Pair : Map)
		{
			count++;
		}
		WWISE_CHECK(count == 2);
	}
	
	SECTION("MultiMap")
	{
		WwiseDBMultiMap<int, int> MultiMap;
		
		//Adding multiple values for a key.
		MultiMap.Add(1, 1);
		MultiMap.Add(1, 2);
		WwiseDBArray<int> OutValues;
		MultiMap.MultiFind(1, OutValues);
		WWISE_CHECK(OutValues.Size() == 2);
		for(unsigned int i = 0; i < OutValues.Size(); i++)
		{
			WWISE_CHECK(OutValues[i] == i + 1);
		}

		//Iterator Test. It passes through the whole multimap.
		int count = 0;
		for(auto& Pair : MultiMap)
		{
			count++;
		}
		WWISE_CHECK(count == 2);

		//Searching for a Key that does not exist. Returns empty array.
		OutValues.Empty();
		MultiMap.MultiFind(2, OutValues);
		WWISE_CHECK(OutValues.Size() == 0);

		//Emptying the MultiMap.
		MultiMap.Empty();
		WWISE_CHECK(MultiMap.Size() == 0);

		//Searching with Pointers.
		MultiMap.Add(1, 1);
		WwiseDBArray<const int*> OutPointerValues;
		MultiMap.MultiFindPointer(1, OutPointerValues);
		WWISE_CHECK(OutPointerValues.Size() == 1);

		OutPointerValues.Empty();
		MultiMap.Empty();
		MultiMap.MultiFindPointer(1, OutPointerValues);
		WWISE_CHECK(OutValues.Size() == 0);
	}

	SECTION("Array")
	{
		WwiseDBArray<int> Array;
		//Adding Values to the Array
		Array.Add(1);
		WWISE_CHECK(Array[0] == 1);
		Array.Add(2);
		WWISE_CHECK(Array[1] == 2);
		WWISE_CHECK(Array.Size() == 2);

		//Iterator Test. It passes through the whole array.
		int count = 0;
		for(auto& Pair : Array)
		{
			count++;
		}
		WWISE_CHECK(count == 2);

		//GetData returns the array as pointer.
		WWISE_CHECK(Array.GetData()[0] == 1);

		//GetData of an empty array returns nullptr
		Array.Empty();
		WWISE_CHECK(!Array.GetData());
		
		//Emptying the Array.
		Array.Empty();
		WWISE_CHECK(Array.Size() == 0);
	}

	SECTION("Set")
	{
		WwiseDBSet<int> Set1;
		WwiseDBSet<int> Set2;
		Set1.Add(1);
		Set1.Add(2);
		Set2.Add(1);

		//Iterator Test. It passes through the whole set.
		int count = 0;
		for(auto& Object : Set1)
		{
			count++;
		}
		WWISE_CHECK(count == 2);
		
		WWISE_CHECK(Set1.Size() == 2);

		//Difference returns the values that are exclusively in one of the two sets
		auto Diff = Set1.Difference(Set2);
		WWISE_CHECK(Diff.Size() == 1);
		WWISE_CHECK(Diff.GetFirst() == 2);

		//Convert Set as Array
		auto Array = Set1.AsArray();
		WWISE_CHECK(Array.Size() == 2);
		WWISE_CHECK(Array[0] == 1);
		WWISE_CHECK(Array[1] == 2);

		//Emptying the Set.
		Set1.Empty();
		WWISE_CHECK(Set1.Size() == 0);
	}

	SECTION("Pair")
	{
		WwiseDBPair<int, int> Pair(1, 2);
		WwiseDBPair<int, int> Pair2(2, 2);
		WwiseDBPair<int, int> Pair3(1, 3);
		WwiseDBPair<int, int> Pair4(1, 2);
		WwiseDBPair<int, int> Pair5(0, 4);


		//Pair Values
		WWISE_CHECK(Pair.GetFirst() == 1);
		WWISE_CHECK(Pair.GetSecond() == 2);

		//Operator == returns true only when both values of the pair are the same
		WWISE_CHECK(Pair != Pair2);
		WWISE_CHECK(Pair != Pair3);
		WWISE_CHECK(Pair == Pair4);
		WWISE_CHECK(Pair != Pair5);

		//Operator < returns true if the first value is smaller.
		//If the first values are equal, check the second value as well.
		WWISE_CHECK(Pair < Pair2);
		WWISE_CHECK(Pair < Pair3);
		WWISE_CHECK(!(Pair < Pair4));
		WWISE_CHECK(!(Pair < Pair5));
	}

	SECTION("String")
	{
		WwiseDBString String("Test");
		WwiseDBString Path("Path/To/../To/Test.exe");

		WWISE_CHECK(!String.Equals(Path, EStringCompareType::CaseSensitive));
		WWISE_CHECK(!String.Equals(Path, EStringCompareType::IgnoreCase));

		//CaseSensitive returns true if the strings are identical
		//IgnoreCase returns true if the characters are the same regardless of Cases.
		WwiseDBString String2("tESt");
		WWISE_CHECK(!String.Equals(String2, EStringCompareType::CaseSensitive));
		WWISE_CHECK(String.Equals(String2, EStringCompareType::IgnoreCase));

		//Number of characters in the String
		WWISE_CHECK(String.Size() == 4);

		//The extension of the path. Returns an empty string if there is no extension.
		WWISE_CHECK(Path.GetExtension() == WwiseDBString("exe"));
		WWISE_CHECK(String.GetExtension() == WwiseDBString(""));

		//The path before the file. Returns an empty string if there is no path before the file.
		WWISE_CHECK(Path.GetPath() == WwiseDBString("Path/To/../To"));
		WWISE_CHECK(String.GetPath() == WwiseDBString(""));

		//The name of the file in the path (with extension). Returns the string if there is path or extension.
		WWISE_CHECK(Path.GetFileName() == WwiseDBString("Test.exe"));
		WWISE_CHECK(String.GetFileName() == WwiseDBString("Test"));

		//CollapseRelativeDirectories fuses ../ with the parent folder above.
		Path.CollapseRelativeDirectories();
		WWISE_CHECK(Path == WwiseDBString("Path/To/Test.exe"));

		WwiseDBString Path2 = WwiseDBString("Path/../To/To/Test.exe");
		Path2.CollapseRelativeDirectories();
#if UE_5_7_OR_LATER
		WWISE_CHECK(Path2 == WwiseDBString("To/To/Test.exe"));
#else
		WWISE_CHECK(Path2 == WwiseDBString("/To/To/Test.exe"));
#endif
		//Collapsing with no ../
		String.CollapseRelativeDirectories();
		WWISE_CHECK(String == WwiseDBString("Test"));

		//Collapsing multiple ../
		WwiseDBString Path3("Path/To/../../To/Test.exe");
		Path3.CollapseRelativeDirectories();
#if UE_5_7_OR_LATER
		WWISE_CHECK(Path3 == WwiseDBString("To/Test.exe"));
#else
		WWISE_CHECK(Path3 == WwiseDBString("/To/Test.exe"));
#endif
		//Collapsing two instances of ../
		WwiseDBString Path4("Path/../To/../To/Test.exe");
		Path4.CollapseRelativeDirectories();
#if UE_5_7_OR_LATER
		WWISE_CHECK(Path4 == WwiseDBString("To/Test.exe"));
#else
		WWISE_CHECK(Path4 == WwiseDBString("/To/Test.exe"));
#endif
		//Adding too much ../ to be removed. Remove the possibles ../ while keeping the others
		WwiseDBString Path5("Path/../../To/Test.exe");
		Path5.CollapseRelativeDirectories();
#if UE_5_7_OR_LATER
		WWISE_CHECK(Path5 == WwiseDBString("../To/Test.exe"));
#else
		WWISE_CHECK(Path5 == WwiseDBString("/../To/Test.exe"));
#endif
		//operator / pairs two string with a '/'
		WWISE_CHECK(String / WwiseDBString("Test2") == WwiseDBString("Test/Test2"));


		WwiseDBString RelativePath("C:/Path");
#ifdef WIN32
		WWISE_CHECK(!RelativePath.IsRelative());
#else
		WWISE_CHECK(RelativePath.IsRelative());
#endif

		WwiseDBString RelativePath2("/Path/To/Test");
#ifdef WIN32
#ifdef WWISE_DB_UNREAL_TYPES
		WWISE_CHECK(!RelativePath2.IsRelative());
#else
		WWISE_CHECK(RelativePath2.IsRelative());
#endif
#else
		WWISE_CHECK(!RelativePath2.IsRelative());
#endif
		WwiseDBString RelativePath3("~/Path");
#ifdef WIN32
		WWISE_CHECK(RelativePath3.IsRelative());
#else
		WWISE_CHECK(!RelativePath3.IsRelative());
#endif

		WwiseDBString RelativePath4("\\\\Path/To/External");
#ifdef WIN32
		WWISE_CHECK(!RelativePath4.IsRelative());
#else
		WWISE_CHECK(RelativePath4.IsRelative());
#endif

		WwiseDBString RelativePath5("/");
#ifdef WIN32
		WWISE_CHECK(!RelativePath5.IsRelative());
#else
		WWISE_CHECK(!RelativePath5.IsRelative());
#endif

		WwiseDBString RelativePath6("Path/To/Test");
#ifdef WIN32
		WWISE_CHECK(RelativePath6.IsRelative());
#else
		WWISE_CHECK(RelativePath6.IsRelative());
#endif

		WwiseDBString RelativePath7("C:RelativePath");
#ifdef WIN32
#ifdef WWISE_DB_UNREAL_TYPES
		WWISE_CHECK(!RelativePath7.IsRelative());
#else
		WWISE_CHECK(RelativePath7.IsRelative());
#endif
#else
#ifdef WWISE_DB_UNREAL_TYPES
		WWISE_CHECK(!RelativePath7.IsRelative());
#else
		WWISE_CHECK(RelativePath7.IsRelative());
#endif
#endif
	}
}