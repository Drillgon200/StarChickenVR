#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include "StarChicken.h"

// It is actually possible to initialize with no import libraries (see link)
// However, it requires going through a lot of undocumented windows stuff that could change at any time
// and I don't want my program to break on some future version of windows just because I didn't want to link to windows at compile time
// https://hero.handmade.network/forums/code-discussion/t/129-howto_-_building_without_import_libraries
extern "C" void mainCRTStartup() {
	drill_lib_init();
	int result = StarChicken::run_star_chicken();
	ExitProcess(result);
}