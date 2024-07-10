#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Win32.h"
//#include <ws2tcpip.h>
#include "StarChicken.h"
#ifdef TESTING_ENABLE
#include "../tests/StarChickenTests.h"
#endif


// It is actually possible to initialize with no import libraries (see link)
// However, it requires going through a lot of undocumented windows stuff that could change at any time
// and I don't want my program to break on some future version of windows just because I didn't want to link to windows at compile time
// https://hero.handmade.network/forums/code-discussion/t/129-howto_-_building_without_import_libraries
extern "C" void __stdcall mainCRTStartup() {
	const U32 failToInitializeDrillLib = 2;
	U32 result = failToInitializeDrillLib;
	if (drill_lib_init()) {
		PNG::init_loader();
#ifdef TESTING_ENABLE
		StarChickenTests::run_all();
#else
		result = StarChicken::run_star_chicken();
#endif
	}
	ExitProcess(result);
}