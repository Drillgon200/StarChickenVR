#pragma once
#include "DrillLib.h"
#include "SerializeTools.h"

namespace StarChicken {

#define PROJECTION_NEAR_PLANE 0.05F

extern U64 frameNumber;
extern F64 deltaTime;
extern F64 totalTime;

extern B32 shouldShutDown;
extern B32 shouldUseDesktopWindow;

extern B32 isInEditorMode;

}