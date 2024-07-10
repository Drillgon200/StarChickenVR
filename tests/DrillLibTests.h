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