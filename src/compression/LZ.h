#pragma once

#include "../DrillLib.h"

namespace LZ {
	
const U32 LZ_WINDOW_SIZE = 1 << 15;
const U32 LZ_MAGIC = 0x5A4C5244; // DRLZ

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
		printf("Huff time: %\nLZ time: %\n"a, lzDecodeStart - huffDecodeStart, current_time_seconds() - lzDecodeStart);
	}
	return result;
}

DEBUG_OPTIMIZE_OFF

}