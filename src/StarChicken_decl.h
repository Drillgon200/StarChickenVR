#pragma once
#include "DrillLib.h"
namespace StarChicken {

#define PROJECTION_NEAR_PLANE 0.05F

	extern u64 frameNumber;
	extern f64 deltaTime;
	extern f64 totalTime;

	extern b32 shouldShutDown;
	extern b32 shouldUseDesktopWindow;
}