#pragma once

#include "../src/DrillLib.h"
#include "../src/UI.h"
#include "Testing.h"

namespace UITests {
using namespace Testing;

void f64_expr_basic() {
	TEST_EXPECT(epsilon_eq(UI::f64_expr_eval("1.1234"a), 1.1234, 0.00001));
	TEST_EXPECT(epsilon_eq(UI::f64_expr_eval("2 + 4"a), 6.0, 0.00001));
	TEST_EXPECT(epsilon_eq(UI::f64_expr_eval("1 + 2 * 3 + 4"a), 11.0, 0.00001));
	TEST_EXPECT(epsilon_eq(UI::f64_expr_eval("1 + 15 / (7 + 8) + 2"a), 4.0, 0.00001));
	TEST_EXPECT(epsilon_eq(UI::f64_expr_eval("1 + afds"a), 1.0, 0.00001));
	TEST_EXPECT(epsilon_eq(UI::f64_expr_eval("a + 1"a), 0.0, 0.00001));
	TEST_EXPECT(epsilon_eq(UI::f64_expr_eval(""a), 0.0, 0.00001));
}

}