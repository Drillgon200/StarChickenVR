#pragma once

#include "DrillLibTests.h"

namespace StarChickenTests {

void run_all() {
	MEMORY_ARENA_FRAME(get_scratch_arena()) {
		TEST_GROUP("DrillLib"a) {
			TEST_GROUP("ArenaArrayList"a) {
				DrillLibTests::ArenaArrayList_basic();
			}
			TEST_GROUP("strafmt"a) {
				DrillLibTests::strafmt_basic();
			}
		}
	}
}

}