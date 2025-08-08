#pragma once

#include "DrillLibTests.h"
#include "ShaderCompilerTests.h"

namespace StarChickenTests {

void run_all() {
	MEMORY_ARENA_FRAME(get_scratch_arena()) {
		TEST_GROUP("DrillLib"a) {
			TEST_GROUP("ArenaArrayList"a) {
				DrillLibTests::ArenaArrayList_basic();
			}
			TEST_GROUP("ArenaHashMap"a) {
				DrillLibTests::ArenaHashMap_basic();
				DrillLibTests::ArenaHashMap_stress();
			}
			TEST_GROUP("strafmt"a) {
				DrillLibTests::strafmt_basic();
			}
		}
		TEST_GROUP("Shader Compiler"a) {
			ShaderCompilerTests::solo_test();
			//ShaderCompilerTests::general_tests();
		}
	}
}

}