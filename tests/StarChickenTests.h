#pragma once

#include "DrillLibTests.h"
#include "ShaderCompilerTests.h"
#include "CompressionTests.h"

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
		TEST_GROUP("BC7 Compression"a) {
			//CompressionTests::bc7_basic();
			//CompressionTests::bc7_2k();
		}
		TEST_GROUP("Huffman Coding"a) {
			TEST_GROUP("5 Elements, Short"a) {
				CompressionTests::huffman_5_elements_short();
			}
			TEST_GROUP("5 Elements, Long"a) {
				CompressionTests::huffman_5_elements_long_random();
			}
			TEST_GROUP("1 Element, Long"a) {
				CompressionTests::huffman_1_element_long();
			}
			TEST_GROUP("Tree Limiting"a) {
				CompressionTests::huffman_tree_limit();
			}
			TEST_GROUP("Random Stress"a) {
				CompressionTests::huffman_random_stress();
			}
		}
		TEST_GROUP("LZ Coding"a) {
			TEST_GROUP("Basic"a) {
				CompressionTests::lz_basic();
			}
			TEST_GROUP("Long Runs"a) {
				CompressionTests::lz_long_runs();
			}
			TEST_GROUP("Random 4 Characters"a) {
				CompressionTests::lz_random4();
			}
		}
	}
}

}