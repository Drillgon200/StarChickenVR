#pragma once
#include "DrillLib.h"

namespace ResourceLoading {

void cleanup_textures();
void destroy_materials();
VkDeviceAddress get_materials_gpu_address();
void flush_materials_memory();

}