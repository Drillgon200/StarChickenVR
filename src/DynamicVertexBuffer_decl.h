#pragma once
#include "DrillLib.h"

namespace DynamicVertexBuffer {

struct Tessellator;

void init();
void destroy();

Tessellator& get_tessellator();
VkDeviceAddress get_gpu_address(Tessellator&);

}