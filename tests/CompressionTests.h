#pragma once

#include "../src/DrillLib.h"
#include "../src/PNG.h"
#include "../src/compression/BC7.h"
#include "../src/compression/Huffman.h"
#include "../src/compression/LZ.h"
#include "Testing.h"

namespace CompressionTests {
using namespace Testing;

void bc7_basic() {
	MemoryArena& arena = get_scratch_arena();
	MEMORY_ARENA_FRAME(arena) {
		RGBA8* img = nullptr;
		U32 widthPx, heightPx;
		PNG::read_image(arena, &img, &widthPx, &heightPx, "resources/textures/bc7_basic_test.png"a);
		U32 blockWidth = (widthPx + 3) / 4;
		U32 blockHeight = (heightPx + 3) / 4;
		U32 blockCount = blockWidth * blockHeight;
		TEST_EXPECT(img != nullptr);
		F32 error{};
		BC7::BC7Block* blocks = BC7::compress_bc7(arena, img, widthPx, heightPx, 1, &error);
		printf("Final Error: %\n"a, error);
		printf("PSNR: 10 * log_10(1 / %)\n"a, error);
		RGBA8* decompressed = BC7::decompress_bc7(arena, blocks, widthPx, heightPx);
		PNG::write_image("compression_tests/bc7_basic_test.png"a, decompressed, widthPx, heightPx);
		write_data_to_file("compression_tests/bc7_basic.bc7"a, blocks, blockCount * sizeof(BC7::BC7Block));
	}
}

void bc7_2k() {
	MemoryArena& arena = get_scratch_arena();
	MEMORY_ARENA_FRAME(arena) {
		RGBA8* img = nullptr;
		U32 widthPx, heightPx;
		PNG::read_image(arena, &img, &widthPx, &heightPx, "resources/textures/cannon_ARM.png"a);
		U32 blockWidth = (widthPx + 3) / 4;
		U32 blockHeight = (heightPx + 3) / 4;
		U32 blockCount = blockWidth * blockHeight;
		TEST_EXPECT(img != nullptr);
		F32 error{};
		BC7::BC7Block* blocks = BC7::compress_bc7(arena, img, widthPx, heightPx, 1, &error);
		printf("Final Error: %\n"a, error);
		printf("PSNR: 10 * log_10(1 / %)\n"a, error);
		RGBA8* decompressed = BC7::decompress_bc7(arena, blocks, widthPx, heightPx);
		PNG::write_image("compression_tests/cannon_ARM.png"a, decompressed, widthPx, heightPx);
		write_data_to_file("compression_tests/cannon_ARM.bc7"a, blocks, blockCount * sizeof(BC7::BC7Block));
	}
}

bool huffman_test_encode_decode_data(MemoryArena& arena, Byte* src, U32 srcLen) {
	bool success = true;
	U32 encodedLen;
	Byte* encoded = Huffman::encode(arena, &encodedLen, src, srcLen);
	TEST_EXPECT(encoded != nullptr);
	success &= encoded != nullptr;
	if (encoded) {
		U32 decodedLen;
		Byte* decoded = Huffman::decode(arena, &decodedLen, encoded, encodedLen);
		TEST_EXPECT(decoded != nullptr);
		success &= decoded != nullptr;
		if (decoded) {
			bool same = decodedLen == srcLen && memcmp(decoded, src, srcLen) == 0;
			TEST_EXPECT(same);
			success &= same;
		}
	}
	return success;
}

void huffman_5_elements_short() {
	MemoryArena& arena = get_scratch_arena();
	MEMORY_ARENA_FRAME(arena) {
		// Should make a tree like
		/*
		   Root
		   / \
		  E  /\
		    D /\
		     C /\
		      A  B
		*/
		StrA data = "ABBBCCCDDDDEEEEEEEE"a;
		U32 encodedLen;
		Byte* encoded = Huffman::encode(arena, &encodedLen, (Byte*)data.str, data.length);
		TEST_EXPECT(encoded != nullptr);
		if (encoded) {
			U8* lenTable = encoded + sizeof(U32) + Huffman::HUFFMAN_PARALLEL_STREAMS * sizeof(U32);
			U32 aLen = lenTable['A' >> 1] >> ('A' & 1) * 4 & 0xF;
			U32 bLen = lenTable['B' >> 1] >> ('B' & 1) * 4 & 0xF;
			U32 cLen = lenTable['C' >> 1] >> ('C' & 1) * 4 & 0xF;
			U32 dLen = lenTable['D' >> 1] >> ('D' & 1) * 4 & 0xF;
			U32 eLen = lenTable['E' >> 1] >> ('E' & 1) * 4 & 0xF;
			TEST_EXPECT(aLen == 4 && bLen == 4 && cLen == 3 && dLen == 2 && eLen == 1);
			bool anyOtherLen = false;
			for (U32 i = 0; i < 256; i++) {
				if (i >= 'A' && i <= 'E') {
					continue;
				}
				U32 len = lenTable[i >> 1] >> (i & 1) * 4 & 0xF;
				anyOtherLen |= len != 0;
			}
			TEST_EXPECT(anyOtherLen == false);
			U64 huffData = 0b00000000010101010110110111111111111110111ull;
			U64 mask =    0b11111111111111111111111111111111111111111ull;
			// This test it outdated given the 4 stream encoder
			//TEST_EXPECT((LOAD_LE64(lenTable + 128) & mask) == huffData);

			U32 decodedLen;
			Byte* decoded = Huffman::decode(arena, &decodedLen, encoded, encodedLen);
			TEST_EXPECT(decoded != nullptr);
			if (decoded) {
				TEST_EXPECT(decodedLen == data.length && memcmp(decoded, data.str, data.length) == 0);
			}
		}
	}
}

void huffman_5_elements_long_random() {
	MemoryArena& arena = get_scratch_arena();
	MEMORY_ARENA_FRAME(arena) {
		U32 srcLen = 454334;
		Byte* data = arena.alloc<Byte>(srcLen);
		Xoshiro256 random; random.seed(1337);
		for (U32 i = 0; i < srcLen; i++) {
			data[i] = random.next() % 5 + 'A';
		}
		huffman_test_encode_decode_data(arena, data, srcLen);
	}
}

void huffman_1_element_long() {
	MemoryArena& arena = get_scratch_arena();
	MEMORY_ARENA_FRAME(arena) {
		const U32 srcLen = 54154;
		Byte* data = arena.alloc<Byte>(srcLen);
		memset(data, 'A', srcLen);
		huffman_test_encode_decode_data(arena, data, srcLen);
	}
}

void huffman_tree_limit() {
	MemoryArena& arena = get_scratch_arena();
	MEMORY_ARENA_FRAME(arena) {
		Byte* data = arena.alloc<Byte>(0);
		U32 srcLen = 0;
		// This data pattern will build a linked list style tree down to HUFFMAN_MAX_DEPTH, then add some low frequency elements past HUFFMAN_MAX_DEPTH
		for (U32 i = 0; i < Huffman::HUFFMAN_MAX_DEPTH; i++) {
			for (U32 j = 0; j < 1 << Huffman::HUFFMAN_MAX_DEPTH - i + 2; j++) {
				data[srcLen++] = i;
			}
		}
		data[srcLen++] = 64;
		data[srcLen++] = 65;
		data[srcLen++] = 66;
		data[srcLen++] = 67;
		arena.stackPtr += srcLen;
		huffman_test_encode_decode_data(arena, data, srcLen);
		
		// This data pattern is similar, but will not have enough elements to demote to perfectly satisfy the elements past HUFFMAN_MAX_DEPTH,
		// forcing the algorithm to promote some elements
		Byte* maxMinusOneStart = data + srcLen - 4 - 8 - 16;
		// Change half of the HUFFMAN_MAX_DEPTH - 1 values into a different number, turning it into two entries at HUFFMAN_MAX_DEPTH
		for (U32 i = 0; i < 8; i++) {
			maxMinusOneStart[i] = 100;
		}
		srcLen -= 2;
		huffman_test_encode_decode_data(arena, data, srcLen);
	}
}

void huffman_random_stress() {
	MemoryArena& arena = get_scratch_arena();
	Xoshiro256 random{}; random.seed(1773);
	U32 randomTestCount = 100;
	U32 randomSizeRange = 1000000;
	bool randomNumbersSuccess = true;
	for (U32 i = 0; i < randomTestCount; i++) {
		MEMORY_ARENA_FRAME(arena) {
			U32 dataSize = random.next() % randomSizeRange;
			U8* data = arena.alloc<U8>(dataSize);
			for (U32 i = 0; i < dataSize; i++) {
				data[i] = U8(random.next());
			}
			Testing::testOutputDisabled = true;
			randomNumbersSuccess &= huffman_test_encode_decode_data(arena, data, dataSize);
			Testing::testOutputDisabled = false;
		}
	}
	TEST_EXPECT(randomNumbersSuccess);

	random.seed(0x7E57);
	bool randomRunsSuccess = true;
	for (U32 i = 0; i < randomTestCount; i++) {
		MEMORY_ARENA_FRAME(arena) {
			U8* data = arena.alloc<U8>(0);
			U32 dataSize = 0;
			for (U32 j = 0; j < 1024; j++) {
				U32 count = random.next() % 100;
				memset(data + dataSize, dataSize, count);
				dataSize += count;
			}
			arena.stackPtr += dataSize;
			Testing::testOutputDisabled = true;
			randomRunsSuccess &= huffman_test_encode_decode_data(arena, data, dataSize);
			Testing::testOutputDisabled = false;
		}
	}
	TEST_EXPECT(randomRunsSuccess);
}

bool lz_test_encode_decode_data(MemoryArena& arena, Byte* src, U32 srcLen) {
	bool success = true;
	U32 encodedLen;
	Byte* encoded = LZ::encode(arena, &encodedLen, src, srcLen);
	TEST_EXPECT(encoded != nullptr);
	success &= encoded != nullptr;
	if (encoded) {
		U32 decodedLen;
		Byte* decoded = LZ::decode(arena, &decodedLen, encoded, encodedLen);
		TEST_EXPECT(decoded != nullptr);
		success &= decoded != nullptr;
		if (decoded) {
			bool same = decodedLen == srcLen && memcmp(decoded, src, srcLen) == 0;
			TEST_EXPECT(same);
			success &= same;
		}
	}
	return success;
}

void lz_basic() {
	MemoryArena& arena = get_scratch_arena();
	MEMORY_ARENA_FRAME(arena) {
		StrA toTest = "aaaaaaaxyzbbbbxbbbb"a;
		lz_test_encode_decode_data(arena, (Byte*)toTest.str, toTest.length);
	}
}

void lz_long_runs() {
	MemoryArena& arena = get_scratch_arena();
	MEMORY_ARENA_FRAME(arena) {
		U32 dataSize = 0;
		Byte* data = arena.alloc<Byte>(0);
		memset(data + dataSize, 5, 100), dataSize += 100;
		memset(data + dataSize, 3, 1000), dataSize += 100;
		data[dataSize++] = 8;
		data[dataSize++] = 9;
		memset(data + dataSize, 3, 10000), dataSize += 100;
		data[dataSize++] = 7;
		arena.stackPtr += dataSize;
		lz_test_encode_decode_data(arena, data, dataSize);
	}
}

void lz_random4() {
	MemoryArena& arena = get_scratch_arena();
	MEMORY_ARENA_FRAME(arena) {
		U32 randomAmount = 10000;
		Xoshiro256 random{}; random.seed(1984);
		Byte* data = arena.alloc<Byte>(randomAmount);
		for (U32 i = 0; i < randomAmount; i++) {
			data[i] = (Byte)random.next() % 4;
		}
		lz_test_encode_decode_data(arena, data, randomAmount);
	}
}

void lz_bc7_full_test() {
	// DEFLATE
	// Windows: 53288 (81.3%)
	// 7z normal: 53701 (81.9%)
	// 7z ultra: 52925 (80.8%)
	// LZMA
	// 7z fast: 50734 (77.4%)
	// 7z ultra: 49635 (75.7%)
	// lin
	// window 15, 16 max matches, greedy: 54546 (83.2%)
	// window 15, 16 matches, optimal: 53595 (81.7%)
	// DRLZ
	// greedy, 2 byte offset: 55272 (84.3%)
	// greedy, log offset: 55967 (85.4%)
	// greedy, log offset, separate match/lit lengths: 55792 (85.1%)
	// greedy, log offset, separate match/lit lengths, hash every pos: 54510 (83.1%)
	// greedy, 2 byte offset, separate match/lit lengths, hash every pos: 53636 (81.8%)
	// optimal, 2 byte offset: 53386 (81.4%)
	//
	// Cannon BaseColor:
	// DEFLATE
	// 7z ultra: 2681950 (63.9%)
	// LZMA
	// 7z ultra: 2192742 (52.3%)
	// DRLZ
	// optimal, 2 byte offset: 2704173 (64.4%)
	// 11 bit huff: 2706500 (64.5%)
	//
	// Cannon Normal:
	// DEFLATE
	// 7z ultra: 1922169 (45.8%)
	// LZMA
	// 7z ultra: 1660809 (39.6%)
	// DRLZ
	// optimal, 2 byte offset: 1921927 (45.8%)
	// 11 bit huff: 1923846 (45.9%)
	//
	// Cannon ARM:
	// DEFLATE
	// 7z ultra: 3180020 (75.8%)
	// LZMA
	// 7z ultra: 2891905 (68.9%)
	// DRLZ
	// optimal, 2 byte offset: 3186797 (76.0%)
	// 11 bit huff: 3190705 (76.1%)
	MemoryArena& arena = get_scratch_arena();
	U32 fileLen;
	Byte* file = read_full_file_to_arena<Byte>(&fileLen, arena, "compression_tests/cannon_Normal.bc7"a);
	scratchArena0.commit_bytes(100 * MEGABYTE);
	scratchArena1.commit_bytes(100 * MEGABYTE);
	U32 encodedLen;
	Byte* encoded = LZ::encode(arena, &encodedLen, file, fileLen);
	printf("File: %\nEncoded: %\n"a, fileLen, encodedLen);
	U32 decodeIterations = 10;
	for (U32 i = 0; i < decodeIterations; i++) {
		MEMORY_ARENA_FRAME(arena) {
			F64 t = current_time_seconds();
			U32 decodedLen;
			Byte* decoded = LZ::decode(arena, &decodedLen, encoded, encodedLen);
			F64 t2 = current_time_seconds();
			if (fileLen != decodedLen || memcmp(file, decoded, fileLen) != 0) {
				__debugbreak();
			}
			// Baseline debug mode: 170 MBps
			// Baseline optimized: 240 MBps (bad measurement, probably twice as fast in reality)
			// Unrolled, amortized store, bextr: 730 MBps. LZ time: 0.0019375999982003123
			// Unrolled, bextr huff time: 0.0035723999972105958
			// LZ no debug: 0.0017833999991125893
			// LZ basic byte loop: 0.0030022000028111506
			// LZ small load branch: 0.001291799999307841
			// Huff multistream x4 ASM with LZ small load optimization: 2100 MBps
			printf("Time taken: %, % MBps\n"a, t2 - t, F64(decodedLen) / (t2 - t) / F64(MEGABYTE));
		}
	}
}

void test_huff_throughput() {
	MemoryArena& arena = get_scratch_arena();
	U32 fileLen;
	Byte* file = read_full_file_to_arena<Byte>(&fileLen, arena, "compression_tests/cannon_Normal.bc7"a);
	U32 encodedLen;
	Byte* encoded = Huffman::encode(arena, &encodedLen, file, fileLen);
	scratchArena0.commit_bytes(100 * MEGABYTE);
	scratchArena1.commit_bytes(100 * MEGABYTE);
	F64 bestThroughput = 0.0F;
	F64 avgThroughput = 0.0F;
	U32 decodeIterations = 1000;
	for (U32 i = 0; i < decodeIterations; i++) {
		MEMORY_ARENA_FRAME(arena) {
			F64 t = current_time_seconds();
			U32 decodedLen;
			Byte* decoded = Huffman::decode(arena, &decodedLen, encoded, encodedLen);
			F64 t2 = current_time_seconds();
			/*if (fileLen != decodedLen || memcmp(file, decoded, fileLen) != 0) {
				__debugbreak();
			}*/
			F64 throughput = F64(decodedLen) / (t2 - t) / F64(MEGABYTE);

			bestThroughput = max(bestThroughput, throughput);
			avgThroughput += throughput;
		}
	}
	avgThroughput /= decodeIterations;
	printf("Throughput avg: %\nThroughput max: %\n"a, avgThroughput, bestThroughput);
}

}