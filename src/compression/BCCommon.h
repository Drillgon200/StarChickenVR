#pragma once
#include "../DrillLib.h"

namespace BCCommon {

RGBA8 decode_5_6_5(U16 compressed) {
	// Extract the quantized bits
	U8 r = compressed >> 11;
	U8 g = (compressed >> 5) & 0b111111;
	U8 b = compressed & 0b11111;
	// Expand 5 to 8 bits by putting the 5 bits in the most significant, then putting the most significant of the 5 bits in the remaining 3 bits
	r = (r << 3) | (r >> 2);
	// Same but with 6 bits instead;
	g = (g << 2) | (g >> 4);
	// Same as 1
	b = (b << 3) | (b >> 2);
	return RGBA8{ r, g, b, 255 };
}

U16 encode_5_6_5(RGB8 data) {
	return ((data.r << 8) & 0b1111100000000000) | ((data.g << 3) & 0b0000011111100000) | ((data.b >> 3) & 0b0000000000011111);
}

U16 encode_5_6_5_f(F32 data[3]) {
	RGB8 rgb;
	rgb.r = U8(clamp01(data[0]) * 255.0F);
	rgb.g = U8(clamp01(data[1]) * 255.0F);
	rgb.b = U8(clamp01(data[2]) * 255.0F);
	return encode_5_6_5(rgb);
}

void quantize_565_f(F32 rgb[3]) {
	// Magic numbers from https://developer.download.nvidia.com/compute/cuda/1.1-Beta/x86_website/projects/dxtc/doc/cuda_dxtc.pdf
	// They found that these factors produced the lowest error. They're pretty close to 1/31 and 1/63 anyway.
	rgb[0] = roundf32(clamp01(rgb[0]) * 31.0F) * 0.03227752766457F;
	rgb[1] = roundf32(clamp01(rgb[1]) * 63.0F) * 0.01583151765563F;
	rgb[2] = roundf32(clamp01(rgb[2]) * 31.0F) * 0.03227752766457F;
}

U32 get_difference(RGB8 rgb1, RGB8 rgb2) {
	// Numbers come from inverse of 0.2989 0.5870 0.1140 color perception weights
	U32 diff = (abs(I32(rgb1.r) - I32(rgb2.r)) * 100) / 335;
	diff += (abs(I32(rgb1.g) - I32(rgb2.g)) * 100) / 170;
	diff += (abs(I32(rgb1.b) - I32(rgb2.b)) * 100) / 877;
	return diff;
}

void get_average3d(F32* outVec, F32* inputVectors, U32 inputCount) {
	U32 inputF32s = inputCount * 3;
	for (U32 i = 0; i < inputF32s; i += 3) {
		outVec[0] += inputVectors[i + 0];
		outVec[1] += inputVectors[i + 1];
		outVec[2] += inputVectors[i + 2];
	}
	F32 invInputCount = 1.0F / static_cast<F32>(inputCount);
	outVec[0] *= invInputCount;
	outVec[1] *= invInputCount;
	outVec[2] *= invInputCount;
}

void principle_component_analysis3d(F32* outMean, F32* outVec, F32* inputVectors, U32 inputCount) {
	F32 average[3]{};
	get_average3d(average, inputVectors, inputCount);
	// Upper triangle of covariance matrix
	F32 covXX = 0.0F;
	F32 covXY = 0.0F;
	F32 covXZ = 0.0F;
	F32 covYY = 0.0F;
	F32 covYZ = 0.0F;
	F32 covZZ = 0.0F;
	// Compute covariance matrix. No need to normalize since we only need the direction from it.
	U32 inputF32s = inputCount * 3;
	for (U32 i = 0; i < inputF32s; i += 3) {
		F32 translatedX = inputVectors[i + 0] - average[0];
		F32 translatedY = inputVectors[i + 1] - average[1];
		F32 translatedZ = inputVectors[i + 2] - average[2];
		covXX += translatedX * translatedX;
		covXY += translatedX * translatedY;
		covXZ += translatedX * translatedZ;
		covYY += translatedY * translatedY;
		covYZ += translatedY * translatedZ;
		covZZ += translatedZ * translatedZ;
	}

	// Power iteration to find the eigenvector we need. Thank god I found this before trying to do the whole cubic formula.
	// http://theory.stanford.edu/~tim/s15/l/l8.pdf
	// This paper says 8 is a good iteration count
	// https://developer.download.nvidia.com/compute/cuda/1.1-Beta/x86_website/projects/dxtc/doc/cuda_dxtc.pdf
	const U32 powerIterations = 8;
	F32 vector[3]{ 1.0F, 1.0F, 1.0F };
	for (U32 i = 0; i < powerIterations; i++) {
		vector[0] = vector[0] * covXX + vector[1] * covXY + vector[2] * covXZ;
		vector[1] = vector[0] * covXY + vector[1] * covYY + vector[2] * covYZ;
		vector[2] = vector[0] * covXZ + vector[1] * covYZ + vector[2] * covZZ;

		F32 rcp = 1.0F / max(vector[0], max(vector[1], vector[2]));
		vector[0] *= rcp;
		vector[1] *= rcp;
		vector[2] *= rcp;
	}
	F32 normalize = 1.0F / sqrtf32(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);
	outVec[0] = vector[0] * normalize;
	outVec[1] = vector[1] * normalize;
	outVec[2] = vector[2] * normalize;
	outMean[0] = average[0];
	outMean[1] = average[1];
	outMean[2] = average[2];
}

F32 squared_error(F32 originalImage[16 * 4], F32 newImage[16 * 4]) {
	F32 error = 0.0F;
	for (U32 pix = 0; pix < 16; pix++) {
		F32 weight = originalImage[pix * 4 + 3];
		for (U32 component = 0; component < 3; component++) {
			F32 diff = originalImage[pix * 4 + component] - newImage[pix * 4 + component];
			error += diff * diff * weight;
		}
		F32 diff = weight - newImage[pix * 4 + 3];
		error += diff * diff * (1.0F + (1.0F - weight) * 3.0F);
	}
	return error;
}

void ordered_permute(U8* order, I32 index, U32 iterations) {
	if (index < 0) {
		printf("% % % %\n"a, order[3], order[2], order[1], order[0]);
	} else {
		for (U32 i = order[index+1]; i < iterations; i++) {
			order[index] = i;
			ordered_permute(order, index - 1, iterations);
		}
	}
}

template<typename T>
void generate_ordered_permutations(I32 iterations, T* out, T (*compressorFunction)(U8[16])) {
	U8 indices[17];
	for (U32 i = 0; i < 16; i++) {
		indices[i] = 0;
	}
	indices[16] = 0;
	I32 index = 15;
	U32 count = 0;
	while (index < 16) {
		if (index < 0) {
			if (out != nullptr) {
				out[count] = compressorFunction(indices);
			}
			count++;
			index++;
			indices[index]++;
		} else {
			if (indices[index] < iterations) {
				index--;
			} else {
				index++;
				indices[index]++;
				for (I32 i = index - 1; i >= 0; i--) {
					indices[i] = indices[index];
				}
			}
		}
	}
}

constexpr U32 bc1PermutationCount = 969;
constexpr U32 bc1AlphaPermutationCount = 153;
U32 bc1Permutations[bc1PermutationCount];
U32 bc1AlphaPermutations[bc1AlphaPermutationCount];

U32 bc1_index_compress(U8 indices[16]) {
	// Since the weights are 0, 1, 1/3, 2/3, the actual order we want is 0, 2, 3, 1
	constexpr U32 remap[4]{ 0, 2, 3, 1 };
	U32 result = 0;
	for (U32 i = 0; i < 16; i++) {
		result = (result << 2) | remap[indices[i]];
	}
	return result;
}

U32 bc1_index_compress_alpha(U8 indices[16]) {
	constexpr U32 remap[3]{ 0, 2, 1 };
	U32 result = 0;
	for (U32 i = 0; i < 16; i++) {
		result = (result << 2) | remap[indices[i]];
	}
	return result;
}

void bc1_index_decompress(U32 compressed, U8 indices[16]) {
	for (I32 i = 15; i >= 0; i--) {
		indices[i] = compressed & 3;
		compressed >>= 2;
	}
}

void compute_compression_index_permutations() {
	generate_ordered_permutations<U32>(4, bc1Permutations, bc1_index_compress);
	generate_ordered_permutations<U32>(3, bc1AlphaPermutations, bc1_index_compress_alpha);
	// U32 test = generate_ordered_permutations<U32>(8, nullptr, nullptr);
}

// outT will contain the min and max T values for this ray intersecting the box. pos must be inside the box. 
void ray_cast_unit_box(F32* outT, F32* pos, F32* dir) {
	F32 invDir[3];
	invDir[0] = 1.0F / dir[0];
	invDir[1] = 1.0F / dir[1];
	invDir[2] = 1.0F / dir[2];

	F32 xTMin = (-pos[0]) * invDir[0];
	F32 xTMax = (1.0F - pos[0]) * invDir[0];
	F32 yTMin = (-pos[1]) * invDir[1];
	F32 yTMax = (1.0F - pos[1]) * invDir[1];
	F32 zTMin = (-pos[2]) * invDir[2];
	F32 zTMax = (1.0F - pos[2]) * invDir[2];

	outT[0] = max(min(xTMin, xTMax), max(min(yTMin, yTMax), min(zTMin, zTMax)));
	outT[1] = min(max(xTMax, xTMin), min(max(yTMax, yTMin), max(zTMax, zTMin)));
}

void find_optimal_endpoints_rgb(F32 pixels[4*16], F32 colorVectors[3 * 16], RGB8* endpoints, F32 mean[3], F32 principleComponent[3], U32 colorCount) {
	if (colorCount == 0) {
		//All transparent block
		endpoints[0] = RGB8{ 0, 0, 0 };
		endpoints[1] = RGB8{ 0, 0, 0 };
		return;
	}

	F32 boxClampT[2];
	ray_cast_unit_box(boxClampT, mean, principleComponent);
	F32 t1 = F32_LARGE;
	F32 t2 = -F32_LARGE;
	for (U32 i = 0; i < colorCount; i++) {
		F32 colorDirX = colorVectors[i * 3 + 0] - mean[0];
		F32 colorDirY = colorVectors[i * 3 + 1] - mean[1];
		F32 colorDirZ = colorVectors[i * 3 + 2] - mean[2];
		F32 t = colorDirX * principleComponent[0] + colorDirY * principleComponent[1] + colorDirZ * principleComponent[2];
		t1 = min(t1, t);
		t2 = max(t2, t);
	}
	t1 = max(t1, boxClampT[0]);
	t2 = min(t2, boxClampT[1]);

	endpoints[0].r = U8(min(max(mean[0] + principleComponent[0] * t1, 0.0F), 1.0F) * 255.0F);
	endpoints[0].g = U8(min(max(mean[1] + principleComponent[1] * t1, 0.0F), 1.0F) * 255.0F);
	endpoints[0].b = U8(min(max(mean[2] + principleComponent[2] * t1, 0.0F), 1.0F) * 255.0F);
	endpoints[1].r = U8(min(max(mean[0] + principleComponent[0] * t2, 0.0F), 1.0F) * 255.0F);
	endpoints[1].g = U8(min(max(mean[1] + principleComponent[1] * t2, 0.0F), 1.0F) * 255.0F);
	endpoints[1].b = U8(min(max(mean[2] + principleComponent[2] * t2, 0.0F), 1.0F) * 255.0F);
}

void fill_pixel_block(RGBA8* image, RGBA8 pixels[16], U32 blockX, U32 blockY, U32 finalWidth, U32 finalHeight) {
	for (U32 pixY = 0; pixY < 4; pixY++) {
		for (U32 pixX = 0; pixX < 4; pixX++) {
			U32 imageIndex = min(blockY * 4 + pixY, finalHeight - 1) * finalWidth + min(blockX * 4 + pixX, finalWidth - 1);
			U32 pixelIndex = pixY * 4 + pixX;
			pixels[pixelIndex] = image[imageIndex];
		}
	}
}

void fill_pixel_blockx8(RGBA8* image, F32* pixels, I32 index, U32 blockX, U32 blockY, U32 finalWidth, U32 finalHeight) {
	// Every 8 indices, move to the next block of 8 4x4 color blocks
	// Add the lower part of index to offset for the in block offset for this pixel block
	U32 blockOffset = (index / 8) * 32 * 16 + index % 8;
	for (U32 pixY = 0; pixY < 4; pixY++) {
		for (U32 pixX = 0; pixX < 4; pixX++) {
			U32 imageIndex = min(blockY * 4 + pixY, finalHeight - 1) * finalWidth + min(blockX * 4 + pixX, finalWidth - 1);
			// 32 F32 stride between each 8 pixel block
			U32 pixelIndex = (pixY * 4 + pixX) * 32 + blockOffset;
			pixels[pixelIndex + 0]  = F32(image[imageIndex].r) / 255.0F;
			pixels[pixelIndex + 8]  = F32(image[imageIndex].g) / 255.0F;
			pixels[pixelIndex + 16] = F32(image[imageIndex].b) / 255.0F;
			pixels[pixelIndex + 24] = F32(image[imageIndex].a) / 255.0F;
		}
	}
}

void copy_block_pixels_to_image(RGBA8* blockPixels, RGBA8* finalImage, U32 blockX, U32 blockY, U32 finalWidth, U32 finalHeight) {
	for (U32 pixY = 0; pixY < 4; pixY++) {
		U32 imgY = blockY * 4 + pixY;
		if (imgY >= finalHeight) {
			continue;
		}
		for (U32 pixX = 0; pixX < 4; pixX++) {
			U32 imgX = blockX * 4 + pixX;
			if (imgX >= finalWidth) {
				continue;
			}
			U32 imageIndex = imgY * finalWidth + imgX;
			U32 pixelIndex = pixY * 4 + pixX;
			finalImage[imageIndex] = blockPixels[pixelIndex];
		}
	}
}

}