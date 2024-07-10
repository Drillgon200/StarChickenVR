#pragma once

#include "../src/DrillLib.h"

namespace Testing {

// A simple testing "framework". I refuse to use google test, even if the code coverage reports are nice, because it's incredibly slow.

#define TEST_EXPECT(val) (Testing::test_expect_check(!(val), __FUNCTION__, __LINE__))
#define TEST_GROUP(name) DEFER_LOOP(Testing::test_group_begin(name), Testing::test_group_end())

struct TestGroup {
	StrA name;
	U32 testsFailed;
	U32 testsPassed;
};

ArenaArrayList<TestGroup> groupStack;

void test_expect_check(B32 failed, const char* funcName, U32 lineNumber) {
	if (failed) {
		groupStack.back().testsFailed++;
		println(strafmt(get_scratch_arena(), "%Test failed, func %, line %"a, "-"a.rep(get_scratch_arena(), groupStack.size), funcName, lineNumber));
	} else {
		groupStack.back().testsPassed++;
		println(strafmt(get_scratch_arena(), "%Test passed"a, "-"a.rep(get_scratch_arena(), groupStack.size)));
	}
}
void test_group_begin(StrA name) {
	println(strafmt(get_scratch_arena(), "%%"a, "-"a.rep(get_scratch_arena(), groupStack.size), name));
	groupStack.push_back(TestGroup{ name });
}
void test_group_end() {
	U32 passed = groupStack.back().testsPassed;
	U32 failed = groupStack.back().testsFailed;
	groupStack.pop_back();
	SetConsoleTextAttribute(consoleOutput, WORD(passed == passed + failed ? FOREGROUND_GREEN : FOREGROUND_RED));
	println(strafmt(get_scratch_arena(), "%%/% tests passed."a, "-"a.rep(get_scratch_arena(), groupStack.size), passed, passed + failed));
	SetConsoleTextAttribute(consoleOutput, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	if (!groupStack.empty()) {
		groupStack.back().testsPassed += passed;
		groupStack.back().testsFailed += failed;
	}
}

}