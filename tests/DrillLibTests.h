#pragma once

#include "../src/DrillLib.h"
#include "Testing.h"

namespace DrillLibTests {
using namespace Testing;

void ArenaArrayList_basic() {
	MemoryArena& testArena = get_scratch_arena_excluding(get_scratch_arena());
	MEMORY_ARENA_FRAME(testArena) {
		ArenaArrayList<U32> list{ &testArena };
		TEST_EXPECT(list.empty());
		TEST_EXPECT(list.size == 0);
		list.push_back(0);
		TEST_EXPECT(!list.empty());
		TEST_EXPECT(list.size == 1);
		list.push_back(1);
		list.push_back(2);
		TEST_EXPECT(list.size == 3);
		TEST_EXPECT(list.data[2] == 2);
		U32* oldDataPointer = list.data;
		U32 oldCapacity = list.capacity;
		while (list.size != list.capacity) {
			list.push_back(7);
		}
		list.push_back(8);
		TEST_EXPECT(oldDataPointer == list.data);
		TEST_EXPECT(list.capacity == oldCapacity * 2);
		TEST_EXPECT(list.data[2] == 2);
		TEST_EXPECT(list.data[oldCapacity] == 8);

		ArenaArrayList<U32> list2 = make_arena_array_list<U32>(testArena, 1, 2, 3);

		while (list.size != list.capacity) {
			list.push_back(4);
		}
		list.push_back(5);
		TEST_EXPECT(oldDataPointer != list.data);
	}
}

void ArenaHashMap_basic() {
	ArenaHashMap<U32, U32> map{ &get_scratch_arena(), 0 };
	TEST_EXPECT(map.empty());
	map.insert(1, 0);
	TEST_EXPECT(map.contains(1));
	TEST_EXPECT(*map.find(1) == 0);
	map.insert(5, 12);
	TEST_EXPECT(!map.empty());
	TEST_EXPECT(map.size == 2);
	TEST_EXPECT(*map.find(5) == 12);
	TEST_EXPECT(*map.find(1) == 0);
	map.remove(1);
	TEST_EXPECT(map.size == 1);
	TEST_EXPECT(map.find(1) == nullptr);
	TEST_EXPECT(*map.find(5) == 12);
}

void ArenaHashMap_stress() {
	ArenaHashMap<U32, U32> map{ &get_scratch_arena(), 0 };
	U32 testAmount = 10000;
	for (U32 i = 1; i <= testAmount; i++) {
		map.insert(i, i + testAmount * 2);
	}
	TEST_EXPECT(map.capacity >= testAmount);
	TEST_EXPECT(map.size == testAmount);
	U32 numFound = 0;
	U32 numCorrect = 0;
	for (U32 i = 1; i <= testAmount; i++) {
		if (U32* found = map.find(i)) {
			numFound++;
			numCorrect += *found == i + testAmount * 2;
		}
	}
	TEST_EXPECT(numFound == testAmount);
	TEST_EXPECT(numCorrect == testAmount);

	U32 numToRemove = testAmount / 2;
	U32 removalOffset = testAmount / 4;
	U32 numRemoved = 0;
	for (U32 i = 1; i <= numToRemove; i++) {
		numRemoved += map.remove(i + removalOffset);
	}
	TEST_EXPECT(numRemoved == numToRemove);
	TEST_EXPECT(map.size == testAmount - numToRemove);

	U32 numCorrectAfterRemovals = 0;
	U32 numIncorrectAfterRemovals = 0;
	U32 numFoundValuesCorrect = 0;
	for (U32 i = 1; i <= testAmount; i++) {
		B32 shouldBePresent = i <= removalOffset || i > removalOffset + numToRemove;
		if (U32* found = map.find(i)) {
			(shouldBePresent ? numCorrectAfterRemovals : numIncorrectAfterRemovals)++;
			numFoundValuesCorrect += *found == i + testAmount * 2;
		} else {
			(shouldBePresent ? numIncorrectAfterRemovals : numCorrectAfterRemovals)++;
		}
	}
	TEST_EXPECT(numCorrectAfterRemovals == testAmount);
	TEST_EXPECT(numFoundValuesCorrect == testAmount - numToRemove);
	TEST_EXPECT(numIncorrectAfterRemovals == 0);
}

void strafmt_basic() {
	StrA fmtNone = strafmt(get_scratch_arena(), "A string with no formatting."a);
	TEST_EXPECT(fmtNone == "A string with no formatting."a);
	StrA fmtStrA = strafmt(get_scratch_arena(), "The % is % in the %."a, "duck"a, "swimming"a, "pond"a);
	TEST_EXPECT(fmtStrA == "The duck is swimming in the pond."a);
	StrA fmtEscape = strafmt(get_scratch_arena(), "This is a % \\%."a, "percent"a);
	TEST_EXPECT(fmtEscape == "This is a percent %."a);
	StrA fmtCStr = strafmt(get_scratch_arena(), "% %"a, "const char pointer", 1234ull);
	TEST_EXPECT(fmtCStr == "const char pointer 1234"a);
	StrA fmtArray = strafmt(get_scratch_arena(), "List of integers: %."a, make_arena_array_list<U32>(get_scratch_arena(), 1, 2, 3, 4, 5));
	TEST_EXPECT(fmtArray == "List of integers: [1, 2, 3, 4, 5]."a);
}

}