#pragma once
#include "DrillLib.h"

namespace ResourceLoading {

extern const U32 MAX_TEXTURE_COUNT;
extern U32 currentTextureMaxCount;
extern U32 currentTextureCount;

void cleanup_textures();
void destroy_materials();
VkDeviceAddress get_materials_gpu_address();
void flush_materials_memory();

}