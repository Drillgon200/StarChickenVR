#pragma once

#include "../DrillLib.h"

struct DLZ2DecodeCallArgs {
	Byte* writePtr;
	U64 matchOrLiteralRunCount;
	Byte* matchOrLiteralSwitches;
	Byte* literals;
	Byte* lengths;
	Byte* offsets;
};
extern "C" void __stdcall dlz2_decode(DLZ2DecodeCallArgs* args);
extern "C" void __stdcall dlz2_decode_full_16_bit_offsets(DLZ2DecodeCallArgs* args);

#define DLZ2_ASM_DECODER
#define DLZ2_FIXED_WIDTH_OFFSETS

namespace LZ {
	
const U32 LZ_WINDOW_SIZE = 1 << 15;
const U32 LZ_MAGIC = 0x5A4C5244; // DRLZ
const U32 LZ2_MAGIC = 0x325A4C44; // DLZ2

#pragma pack(push, 1)
struct LZHeader {
	U32 magic;
	U32 srcLength;
	U32 matchOrLiteralRunCount;
	U32 literalBufferOffset;
	U32 literalBufferEncodedSize;
	U32 literalLengthBufferOffset;
	U32 literalLengthBufferEncodedSize;
	U32 matchLengthBufferOffset;
	U32 matchLengthBufferEncodedSize;
	U32 offsetBufferOffset;
	U32 offsetBufferEncodedSize;
};
struct LZ2Header {
	U32 magic;
	U32 srcLength;
	U32 matchOrLiteralRunCount;
	U32 literalBufferOffset;
	U32 literalBufferEncodedSize;
	U32 lengthBufferOffset;
	U32 lengthBufferEncodedSize;
	U32 offsetBufferOffset;
	U32 offsetBufferEncodedSize;
};
#pragma pack(pop)

Byte* encode(MemoryArena& outputArena, U32* outLen, Byte* data, U32 dataLen) {
	MemoryArena& arena = get_scratch_arena_excluding(outputArena);
	Byte* result = nullptr;
	MEMORY_ARENA_FRAME(arena) {
		struct HashChainEntry {
			U32 firstBytes;
			U32 offset;
			U32 next;
		};
		HashChainEntry* matchEntries = arena.zalloc<HashChainEntry>(LZ_WINDOW_SIZE);
		U16* hashTable = arena.zalloc<U16>(LZ_WINDOW_SIZE);
		U32* bitCostTable = arena.alloc<U32>(dataLen + 1);
		memset(bitCostTable, 0xFF, (dataLen + 1) * sizeof(U32));
		U32* bestOffsetTable = arena.zalloc<U32>(dataLen + 1);
		U32* bestLengthTable = arena.zalloc<U32>(dataLen + 1);
		// Forwards step to find the "optimal" choice at each offset
		for (U32 i = 0; i < dataLen; i++) {
			// Classic hash chain algorithm, like in zlib
			U32 threeBytes = LOAD_LE32(&data[i]) & 0x00FFFFFF;
			U32 hashIdx = hash32(threeBytes) & LZ_WINDOW_SIZE - 1;
			HashChainEntry* lastMatch = &matchEntries[hashTable[hashIdx]];
			for (U32 matchNumber = 0; matchNumber < 16 && lastMatch->firstBytes == threeBytes && i - lastMatch->offset < LZ_WINDOW_SIZE; matchNumber++) {
				U32 actualMatchLength = 0;
				for (U32 matchBefore = lastMatch->offset, matchCurrent = i; matchCurrent < dataLen && data[matchBefore] == data[matchCurrent] && actualMatchLength < 256; matchBefore++, matchCurrent++, actualMatchLength++);
				U32 offset = i - lastMatch->offset;
				// 1-2 bytes for offset, 1 byte for length, 1 bit for match/lit determination. Should be replaced with huffman tree values eventually
				U32 estimatedCost = bitCostTable[i] + (offset > 127 ? 16 : 8) + 8 + 1;
				if (estimatedCost < bitCostTable[i + actualMatchLength]) {
					bitCostTable[i + actualMatchLength] = estimatedCost;
					bestOffsetTable[i + actualMatchLength] = offset;
					bestLengthTable[i + actualMatchLength] = actualMatchLength;
				}
				lastMatch = &matchEntries[lastMatch->next];
			}
			// 1 byte for literal, 1 bit for match/lit determination. Should be replaced with huffman tree values eventually
			U32 estimatedLiteralCost = bitCostTable[i] + 8 + 1;
			if (estimatedLiteralCost <= bitCostTable[i + 1]) {
				bitCostTable[i + 1] = estimatedLiteralCost;
				bestOffsetTable[i + 1] = 0;
				bestLengthTable[i + 1] = 1;
			}
			matchEntries[i & LZ_WINDOW_SIZE - 1] = HashChainEntry{ threeBytes, i, hashTable[hashIdx]};
			hashTable[hashIdx] = i & LZ_WINDOW_SIZE - 1;
		}
		// Backwards step to choose the encoded matches/literals
		U32 literalCount = 0;
		U32 literalLengthCount = 0;
		U32 matchLengthCount = 0;
		U32 offsetCount = 0;
		U32 offsetOrLiteralBufferSize = 0;
		U8* literalBuffer = arena.zalloc<U8>(dataLen);
		U8* literalLengthBuffer = arena.zalloc<U8>(dataLen);
		U8* matchLengthBuffer = arena.zalloc<U8>(dataLen);
		U8* offsetBuffer = arena.zalloc<U8>(dataLen);
		U8* offsetOrLiteralBuffer = arena.zalloc<U8>(dataLen + sizeof(U64));
		U32 literalRunCount = 0;
		U32 totalLiteralOrMatchCount = 0;
		U64 bitBuf = 0;
		U64 bitBufBits = 0;
		for (I32 i = dataLen; i >= 1;) {
			U32 bestOffset = bestOffsetTable[i];
			U32 bestLength = bestLengthTable[i];
			if (bestLength <= 1) {
				// Write literal
				if (++literalRunCount == 256) {
					literalLengthBuffer[dataLen - ++literalLengthCount] = 255;
					literalRunCount = 0;
					bitBufBits++;
					totalLiteralOrMatchCount++;
				}
				literalBuffer[dataLen - ++literalCount] = data[i - 1];
				i--;
			} else {
				if (literalRunCount) {
					literalLengthBuffer[dataLen - ++literalLengthCount] = literalRunCount - 1;
					literalRunCount = 0;
					bitBufBits++;
					totalLiteralOrMatchCount++;
				}
				// Write match
				matchLengthBuffer[dataLen - ++matchLengthCount] = bestLength - 1;
				offsetBuffer[dataLen - ++offsetCount] = bestOffset >= 128 ? bestOffset >> 7 : bestOffset;
				if (bestOffset >= 128) {
					offsetBuffer[dataLen - ++offsetCount] = bestOffset & 0x7F | 0x80;
				}
				bitBuf |= 1ull << 63 - bitBufBits;
				bitBufBits++;
				totalLiteralOrMatchCount++;
				i -= bestLength;
			}
			if (bitBufBits >= 32) {
				bitBufBits -= 32;
				offsetOrLiteralBufferSize += sizeof(U32);
				STORE_LE32(offsetOrLiteralBuffer + dataLen - offsetOrLiteralBufferSize, (U32)(bitBuf >> 32));
				bitBuf <<= 32;
			}
		}
		if (literalRunCount) {
			literalLengthBuffer[dataLen - ++literalLengthCount] = literalRunCount - 1;
			bitBufBits++;
			totalLiteralOrMatchCount++;
		}
		if (bitBufBits != 0) {
			STORE_LE64(offsetOrLiteralBuffer + dataLen - offsetOrLiteralBufferSize - sizeof(U64), bitBuf);
			U32 extraBytes = (bitBufBits + 7) / 8;
			offsetOrLiteralBufferSize += extraBytes;
			U32 excessToShift = extraBytes * 8 - bitBufBits;
			U8* offsetOrLiteralBufferStart = offsetOrLiteralBuffer + dataLen - offsetOrLiteralBufferSize;
			if (excessToShift) {
				// Since we wrote the bits out in reverse, we may have bits we didn't write at the beginning byte. Shift the whole buffer down so those extra bits are at the end instead.
				for (U32 i = 0; i < offsetOrLiteralBufferSize; i += sizeof(U64)) {
					STORE_LE64(offsetOrLiteralBufferStart + i, LOAD_LE64(offsetOrLiteralBufferStart + i) >> excessToShift | U64(offsetOrLiteralBufferStart[i + sizeof(U64)]) << 64 - excessToShift);
				}
			}
		}


		U64 resultOffset = outputArena.stackPtr;
		outputArena.stackPtr += sizeof(LZHeader);
		memcpy(outputArena.stackBase + outputArena.stackPtr, offsetOrLiteralBuffer + dataLen - offsetOrLiteralBufferSize, offsetOrLiteralBufferSize);
		outputArena.stackPtr += offsetOrLiteralBufferSize;
		U32 literalEncodedSize;
		Huffman::encode(outputArena, &literalEncodedSize, literalBuffer + dataLen - literalCount, literalCount);
		U32 literalLengthEncodedSize;
		Huffman::encode(outputArena, &literalLengthEncodedSize, literalLengthBuffer + dataLen - literalLengthCount, literalLengthCount);
		U32 matchLengthEncodedSize;
		Huffman::encode(outputArena, &matchLengthEncodedSize, matchLengthBuffer + dataLen - matchLengthCount, matchLengthCount);
		U32 offsetEncodedSize;
		Huffman::encode(outputArena, &offsetEncodedSize, offsetBuffer + dataLen - offsetCount, offsetCount);
		LZHeader header{};
		header.magic = LZ_MAGIC;
		header.srcLength = dataLen;
		header.matchOrLiteralRunCount = totalLiteralOrMatchCount;
		header.literalBufferOffset = sizeof(LZHeader) + offsetOrLiteralBufferSize;
		header.literalBufferEncodedSize = literalEncodedSize;
		header.literalLengthBufferOffset = header.literalBufferOffset + literalEncodedSize;
		header.literalLengthBufferEncodedSize = literalLengthEncodedSize;
		header.matchLengthBufferOffset = header.literalLengthBufferOffset + literalLengthEncodedSize;
		header.matchLengthBufferEncodedSize = matchLengthEncodedSize;
		header.offsetBufferOffset = header.matchLengthBufferOffset + matchLengthEncodedSize;
		header.offsetBufferEncodedSize = offsetEncodedSize;
		memcpy(outputArena.stackBase + resultOffset, &header, sizeof(header));

		result = outputArena.stackBase + resultOffset;
		*outLen = outputArena.stackPtr - resultOffset;
	}
	return result;
}

// Variant of DRLZ optimized for decode speed
Byte* encode2(MemoryArena& outputArena, U32* outLen, Byte* data, U32 dataLen) {
	MemoryArena& arena = get_scratch_arena_excluding(outputArena);
	Byte* result = nullptr;
	MEMORY_ARENA_FRAME(arena) {
		struct HashChainEntry {
			U32 firstBytes;
			U32 offset;
			U32 next;
		};
		HashChainEntry* matchEntries = arena.zalloc<HashChainEntry>(LZ_WINDOW_SIZE);
		U16* hashTable = arena.zalloc<U16>(LZ_WINDOW_SIZE);
		U32* bitCostTable = arena.alloc<U32>(dataLen + 1);
		memset(bitCostTable, 0xFF, (dataLen + 1) * sizeof(U32));
		U32* bestOffsetTable = arena.zalloc<U32>(dataLen + 1);
		U32* bestLengthTable = arena.zalloc<U32>(dataLen + 1);
		const U32 LENGTH_LIMIT = 32;
		// Forwards step to find the "optimal" choice at each offset
		for (U32 i = 0; i < dataLen; i++) {
			// Classic hash chain algorithm, like in zlib
			// Limit matches to a minimum of LENGTH_LIMIT bytes in reverse
			if (i >= 4) {
				U32 threeBytes = LOAD_LE32(&data[i]) & 0x00FFFFFF;
				U32 hashIdx = hash32(threeBytes) & LZ_WINDOW_SIZE - 1;
				HashChainEntry* lastMatch = &matchEntries[hashTable[hashIdx]];

				for (U32 matchNumber = 0; matchNumber < 16 && lastMatch->firstBytes == threeBytes && i - lastMatch->offset < LZ_WINDOW_SIZE; matchNumber++) {
					U32 actualMatchLength = 0;
					for (U32 matchBefore = lastMatch->offset, matchCurrent = i; matchCurrent < dataLen && data[matchBefore] == data[matchCurrent] && actualMatchLength < LENGTH_LIMIT; matchBefore++, matchCurrent++, actualMatchLength++);
					U32 offset = i - lastMatch->offset;
					// 1-2 bytes for offset, 4 bits for length, 1 bit for match/lit determination. Should be replaced with huffman tree values eventually
					U32 estimatedCost = bitCostTable[i] + (offset > 127 ? 16 : 8) + 4 + 1;
					actualMatchLength = min(actualMatchLength, offset); // Since we always do a single copy instruction for each length, we can't overlap the match with bytes not yet written
					if (estimatedCost < bitCostTable[i + actualMatchLength] && offset >= LENGTH_LIMIT) {
						bitCostTable[i + actualMatchLength] = estimatedCost;
						bestOffsetTable[i + actualMatchLength] = offset;
						bestLengthTable[i + actualMatchLength] = actualMatchLength;
					}
					lastMatch = &matchEntries[lastMatch->next];
				}

				U32 threeBytesPrev = LOAD_LE32(&data[i - 4]) & 0x00FFFFFF;
				U32 hashIdxPrev = hash32(threeBytesPrev) & LZ_WINDOW_SIZE - 1;
				HashChainEntry* lastMatchPrev = &matchEntries[hashTable[hashIdxPrev]];
				matchEntries[i - 4 & LZ_WINDOW_SIZE - 1] = HashChainEntry{ threeBytesPrev, i - 4, hashTable[hashIdxPrev]};
				hashTable[hashIdxPrev] = i - 4 & LZ_WINDOW_SIZE - 1;
			}
			// 1 byte for literal, 1 bit for match/lit determination. Should be replaced with huffman tree values eventually
			U32 estimatedLiteralCost = bitCostTable[i] + 8 + 1;
			if (estimatedLiteralCost <= bitCostTable[i + 1]) {
				bitCostTable[i + 1] = estimatedLiteralCost;
				bestOffsetTable[i + 1] = 0;
				bestLengthTable[i + 1] = 1;
			}
		}
		// Backwards step to choose the encoded matches/literals
		U32 literalCount = 0;
		U32 lengthCount = 0;
		U32 offsetCount = 0;
		U32 offsetOrLiteralBufferSize = 0;
		U8* literalBuffer = arena.zalloc<U8>(dataLen);
		U8* lengthBuffer = arena.zalloc<U8>(dataLen);
		U8* offsetBuffer = arena.zalloc<U8>(dataLen);
		U8* offsetOrLiteralBuffer = arena.zalloc<U8>(dataLen + sizeof(U64));
		U32 literalRunCount = 0;
		U32 totalLiteralOrMatchCount = 0;
		U64 bitBuf = 0;
		U64 bitBufBits = 0;
		F64 avgLengths = 0.0;
		for (I32 i = dataLen; i >= 1;) {
			U32 bestOffset = bestOffsetTable[i];
			U32 bestLength = bestLengthTable[i];
			if (bestLength <= 1) {
				// Write literal (max run count of 16 so we can decode branchless)
				if (++literalRunCount == LENGTH_LIMIT) {
					lengthBuffer[dataLen - ++lengthCount] = LENGTH_LIMIT;
					avgLengths += LENGTH_LIMIT;
					literalRunCount = 0;
					bitBufBits++;
					totalLiteralOrMatchCount++;
				}
				literalBuffer[dataLen - ++literalCount] = data[i - 1];
				i--;
			} else {
				if (literalRunCount) {
					lengthBuffer[dataLen - ++lengthCount] = literalRunCount;
					avgLengths += literalRunCount;
					literalRunCount = 0;
					bitBufBits++;
					totalLiteralOrMatchCount++;
				}
				// Write match
				lengthBuffer[dataLen - ++lengthCount] = bestLength;
				avgLengths += bestLength;
#ifdef DLZ2_FIXED_WIDTH_OFFSETS
				offsetBuffer[dataLen - ++offsetCount] = U8(bestOffset >> 8);
				offsetBuffer[dataLen - ++offsetCount] = U8(bestOffset);
#else
				offsetBuffer[dataLen - ++offsetCount] = bestOffset >= 128 ? U8(bestOffset >> 7) : U8(bestOffset << 1);
				if (bestOffset >= 128) {
					offsetBuffer[dataLen - ++offsetCount] = bestOffset << 1 | 1;
				}
#endif

				bitBuf |= 1ull << 63 - bitBufBits;
				bitBufBits++;
				totalLiteralOrMatchCount++;
				i -= bestLength;
			}
			if (bitBufBits >= 32) {
				bitBufBits -= 32;
				offsetOrLiteralBufferSize += sizeof(U32);
				STORE_LE32(offsetOrLiteralBuffer + dataLen - offsetOrLiteralBufferSize, (U32)(bitBuf >> 32));
				bitBuf <<= 32;
			}
		}
		if (literalRunCount) {
			lengthBuffer[dataLen - ++lengthCount] = literalRunCount;
			avgLengths += literalRunCount;
			bitBufBits++;
			totalLiteralOrMatchCount++;
		}
		avgLengths /= totalLiteralOrMatchCount;
		printf("AVG lengths: %\n"a, avgLengths);
		if (bitBufBits != 0) {
			STORE_LE64(offsetOrLiteralBuffer + dataLen - offsetOrLiteralBufferSize - sizeof(U64), bitBuf);
			U32 extraBytes = (bitBufBits + 7) / 8;
			offsetOrLiteralBufferSize += extraBytes;
			U32 excessToShift = extraBytes * 8 - bitBufBits;
			U8* offsetOrLiteralBufferStart = offsetOrLiteralBuffer + dataLen - offsetOrLiteralBufferSize;
			if (excessToShift) {
				// Since we wrote the bits out in reverse, we may have bits we didn't write at the beginning byte. Shift the whole buffer down so those extra bits are at the end instead.
				for (U32 i = 0; i < offsetOrLiteralBufferSize; i += sizeof(U64)) {
					STORE_LE64(offsetOrLiteralBufferStart + i, LOAD_LE64(offsetOrLiteralBufferStart + i) >> excessToShift | U64(offsetOrLiteralBufferStart[i + sizeof(U64)]) << 64 - excessToShift);
				}
			}
		}


		U64 resultOffset = outputArena.stackPtr;
		outputArena.stackPtr += sizeof(LZ2Header);
		memcpy(outputArena.stackBase + outputArena.stackPtr, offsetOrLiteralBuffer + dataLen - offsetOrLiteralBufferSize, offsetOrLiteralBufferSize);
		outputArena.stackPtr += offsetOrLiteralBufferSize;
		U32 literalEncodedSize;
		Huffman::encode(outputArena, &literalEncodedSize, literalBuffer + dataLen - literalCount, literalCount);
		U32 lengthEncodedSize;
		Huffman::encode(outputArena, &lengthEncodedSize, lengthBuffer + dataLen - lengthCount, lengthCount);
		U32 offsetEncodedSize;
		Huffman::encode(outputArena, &offsetEncodedSize, offsetBuffer + dataLen - offsetCount, offsetCount);
		LZ2Header header{};
		header.magic = LZ2_MAGIC;
		header.srcLength = dataLen;
		header.matchOrLiteralRunCount = totalLiteralOrMatchCount;
		header.literalBufferOffset = sizeof(LZ2Header) + offsetOrLiteralBufferSize;
		header.literalBufferEncodedSize = literalEncodedSize;
		header.lengthBufferOffset = header.literalBufferOffset + literalEncodedSize;
		header.lengthBufferEncodedSize = lengthEncodedSize;
		header.offsetBufferOffset = header.lengthBufferOffset + lengthEncodedSize;
		header.offsetBufferEncodedSize = offsetEncodedSize;
		memcpy(outputArena.stackBase + resultOffset, &header, sizeof(header));

		result = outputArena.stackBase + resultOffset;
		*outLen = outputArena.stackPtr - resultOffset;
	}
	return result;
}

DEBUG_OPTIMIZE_ON

Byte* decode(MemoryArena& outputArena, U32* outLen, Byte* data, U32 dataLen) {
	if (dataLen < sizeof(LZHeader)) {
		printf("Data length too short, corruption?"a);
		return nullptr;
	}
	LZHeader header;
	memcpy(&header, data, sizeof(header));
	if (header.magic != LZ_MAGIC) {
		printf("Invalid data, LZ magic was wrong"a);
		return nullptr;
	}
	Byte* result = outputArena.alloc<Byte>(header.srcLength);
	MEMORY_ARENA_FRAME(outputArena) {
		Byte* matchOrLiteralRunBuffer = data + sizeof(header);
		F64 huffDecodeStart = current_time_seconds();
		U32 literalBufferLen;
		Byte* literalBuffer = Huffman::decode(outputArena, &literalBufferLen, data + header.literalBufferOffset, header.literalBufferEncodedSize);
		U32 literalLengthBufferLen;
		Byte* literalLengthBuffer = Huffman::decode(outputArena, &literalLengthBufferLen, data + header.literalLengthBufferOffset, header.literalLengthBufferEncodedSize);
		U32 matchLengthBufferLen;
		Byte* matchLengthBuffer = Huffman::decode(outputArena, &matchLengthBufferLen, data + header.matchLengthBufferOffset, header.matchLengthBufferEncodedSize);
		U32 offsetBufferLen;
		Byte* offsetBuffer = Huffman::decode(outputArena, &offsetBufferLen, data + header.offsetBufferOffset, header.offsetBufferEncodedSize);
		Byte* ogOffsetBuf = offsetBuffer;

		F64 lzDecodeStart = current_time_seconds();

		*outLen = header.srcLength;
		Byte* writePtr = result;
		U64 bitBuf = LOAD_LE64(matchOrLiteralRunBuffer);
		matchOrLiteralRunBuffer += sizeof(U64);
		U32 bitBufBits = 64;
		for (U32 i = 0; i < header.matchOrLiteralRunCount; i++) {
			U32 count;
			if (bitBuf & 1) { // Match
				count = *matchLengthBuffer++ + 1;
				/*U32 bitsToRead = *offsetBuffer++;
				bitBuf >>= 1;
				U32 offset = (1 << bitsToRead) + (U32(bitBuf) & (1 << bitsToRead) - 1);
				bitBuf >>= bitsToRead;
				bitBufBits -= 1 + bitsToRead;*/

				// The branchless instruction sequence seems to be slightly faster (not by much though)
				U32 tentativeOffset = LOAD_LE16(offsetBuffer++);
				U32 offset = _pext_u32(tentativeOffset, (tentativeOffset & 0x80) != 0 ? 0xFF7F : 0x7F);
				offsetBuffer += (tentativeOffset & 0x80) != 0;
				/*U32 offset = *offsetBuffer++;
				if (offset & 0x80) {
					offset = offset & 0x7F | *offsetBuffer++ << 7;
				}*/
				if (offset > writePtr - result) {
					__debugbreak();
				}
				Byte* fromPtr = writePtr - offset;
				if (count <= 16 && offset >= 16) {
					// 16 bytes appears to be roughly optimal for avoiding the rep movs overhead
					_mm_storeu_si128((__m128i*)writePtr, _mm_loadu_si128((__m128i*)fromPtr));
				} else {
					__movsb(writePtr, fromPtr, count);
				}
			} else { // Literal run
				count = *literalLengthBuffer++ + 1;
				if (count <= 16) {
					_mm_storeu_si128((__m128i*)writePtr, _mm_loadu_si128((__m128i*)literalBuffer));
				} else {
					__movsb(writePtr, literalBuffer, count);
				}
				literalBuffer += count;
			}
			bitBuf >>= 1;
			bitBufBits--;
			writePtr += count;
			if (bitBufBits == 0) {
				bitBuf = LOAD_LE64(matchOrLiteralRunBuffer);
				matchOrLiteralRunBuffer += sizeof(U64);
				bitBufBits = 64;
			}
		}
		// Baseline opt:
		// Huff time: 0.011819200124591589
		// LZ time: 0.007234199903905392 
		F64 lzTime = current_time_seconds() - lzDecodeStart;
		//printf("Huff time: %\nLZ time: %\nLZ throughput: % MBps\n"a, lzDecodeStart - huffDecodeStart, lzTime, header.srcLength / lzTime / MEGABYTE);
	}
	return result;
}

Byte* decode2(MemoryArena& outputArena, U32* outLen, Byte* data, U32 dataLen) {
	if (dataLen < sizeof(LZ2Header)) {
		printf("Data length too short, corruption?"a);
		return nullptr;
	}
	LZ2Header header;
	memcpy(&header, data, sizeof(header));
	if (header.magic != LZ2_MAGIC) {
		printf("Invalid data, LZ magic was wrong"a);
		return nullptr;
	}
	Byte* result = outputArena.alloc<Byte>(header.srcLength);
	MEMORY_ARENA_FRAME(outputArena) {
		Byte* matchOrLiteralRunBuffer = data + sizeof(header);
		F64 huffDecodeStart = current_time_seconds();
		U32 literalBufferLen;
		Byte* literalBuffer = Huffman::decode(outputArena, &literalBufferLen, data + header.literalBufferOffset, header.literalBufferEncodedSize);
		U32 lengthBufferLen;
		Byte* lengthBuffer = Huffman::decode(outputArena, &lengthBufferLen, data + header.lengthBufferOffset, header.lengthBufferEncodedSize);
		U32 offsetBufferLen;
		Byte* offsetBuffer = Huffman::decode(outputArena, &offsetBufferLen, data + header.offsetBufferOffset, header.offsetBufferEncodedSize);

		F64 lzDecodeStart = current_time_seconds();

		*outLen = header.srcLength;
#ifdef DLZ2_ASM_DECODER
		DLZ2DecodeCallArgs args{};
		args.writePtr = result;
		args.matchOrLiteralRunCount = header.matchOrLiteralRunCount;
		args.matchOrLiteralSwitches = matchOrLiteralRunBuffer;
		args.literals = literalBuffer;
		args.lengths = lengthBuffer;
		args.offsets = offsetBuffer;
		// Separated variable length int decode stream: 10300 MBps decode (faster than LZ4 for certain textures!)
		// Fixed 16 bit offsets: 13500 MBps
#ifdef DLZ2_FIXED_WIDTH_OFFSETS
		dlz2_decode_full_16_bit_offsets(&args);
#else
		dlz2_decode(&args);
#endif
#else
		Byte* writePtr = result;
		U64 bitBuf = LOAD_LE64(matchOrLiteralRunBuffer);
		matchOrLiteralRunBuffer += sizeof(U64);
		U32 bitBufBits = 64;
		for (U32 i = 0; i < header.matchOrLiteralRunCount; i++) {
			U32 count = *lengthBuffer++;
			if (bitBuf & 1) { // Match
				// The branchless instruction sequence seems to be slightly faster (not by much though)
				U32 tentativeOffset = LOAD_LE16(offsetBuffer++);
#ifdef DLZ2_FIXED_WIDTH_OFFSETS
				U32 offset = tentativeOffset;
				offsetBuffer++;
#else
				U32 offset = tentativeOffset & 1 ? tentativeOffset >> 1 : (tentativeOffset & 0xFF) >> 1;
				offsetBuffer += tentativeOffset & 1;
#endif
				Byte* fromPtr = writePtr - offset;
				_mm256_storeu_si256((__m256i*)writePtr, _mm256_loadu_si256((__m256i*)fromPtr));
			} else { // Literal run
				_mm256_storeu_si256((__m256i*)writePtr, _mm256_loadu_si256((__m256i*)literalBuffer));
				literalBuffer += count;
			}
			bitBuf >>= 1;
			bitBufBits--;
			writePtr += count;
			if (bitBufBits == 0) {
				bitBuf = LOAD_LE64(matchOrLiteralRunBuffer);
				matchOrLiteralRunBuffer += sizeof(U64);
				bitBufBits = 64;
			}
		}
#endif
		// Baseline opt:
		// Huff time: 0.011819200124591589
		// LZ time: 0.007234199903905392 
		F64 lzTime = current_time_seconds() - lzDecodeStart;
		//printf("Huff time: %\nLZ time: %\nLZ throughput: % MBps\n"a, lzDecodeStart - huffDecodeStart, lzTime, header.srcLength / lzTime / MEGABYTE);
	}
	return result;
}

DEBUG_OPTIMIZE_OFF

void print_length_decode_lookup_tables() {
	// This table allows us to broadcast 8/16 bit variable integers into 16 bit slots
	U64 pdepMaskLookup[256];
	// This table allows us to figure out how many bytes were consumed for a given variable byte code stream and the number of codes that will be consumed
	U64 bytesConsumedLookup[256];
	// This table allows us to broadcast 16 bit integers into the slots they need to be in to line up with the match/literal decoder
	U64 pdepExtendMaskLookup[16];
	for (U32 i = 0; i < 256; i++) {
		U64 pdepMask = 0;
		for (U32 j = 0, k = 0; k < 8; k += 2) {
			if (i & 1 << j) {
				pdepMask |= 0xFFFFull << k * 8;
				j += 2;
			} else {
				pdepMask |= 0x00FFull << k * 8;
				j++;
			}
		}
		pdepMaskLookup[i] = pdepMask;
		printf("%, "a, pdepMask);
		if (i % 8 == 7) {
			printf("\n"a);
		}
	}
	printf("\n\n"a);
	for (U32 i = 0; i < 256; i++) {
		U64 bytesConsumed = 0;
		for (U64 j = 0, currentlyConsumed = 0; j < 5; j++) {
			bytesConsumed |= currentlyConsumed << j * 8;
			currentlyConsumed += i & 1 << currentlyConsumed ? 2 : 1;
		}
		bytesConsumedLookup[i] = bytesConsumed;
		printf("%, "a, bytesConsumed);
		if (i % 8 == 7) {
			printf("\n"a);
		}
	}
	printf("\n\n"a);
	for (U32 i = 0; i < 0b10000; i++) {
		U64 pdepMask = 0;
		for (U32 j = 0; j < 4; j++) {
			if (i & 1 << j) {
				pdepMask |= 0xFFFFull << j * 16;
			}
		}
		pdepExtendMaskLookup[i] = pdepMask;
		printf("%, "a, pdepMask);
		if (i % 8 == 7) {
			printf("\n"a);
		}
	}
}

}