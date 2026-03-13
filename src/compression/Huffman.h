#pragma once

#include "../DrillLib.h"

struct Huff4StreamDecodeCallArgs {
	U16* decodeTable;
	Byte* writePtr;
	U64 outputLen;
	Byte* readPtr0;
	Byte* readPtr1;
	Byte* readPtr2;
	Byte* readPtr3;
};
extern "C" Byte* __stdcall huff_4_stream_decode(Huff4StreamDecodeCallArgs* args);
struct Huff16StreamDecodeCallArgs {
	U16* decodeTable;
	Byte* writePtr;
	U64 outputLen;
	Byte* readPtr;
	U64 readOffset[16];
};
extern "C" Byte* __stdcall huff_16_stream_decode(Huff16StreamDecodeCallArgs* args);

namespace Huffman {

// 11 bit is only marginally (~0.1%) worse than a 12 bit limit for 2k textures and allows 5 decodes without a refill check in the decoder instead of 4.
const U32 HUFFMAN_MAX_DEPTH = 11;
const U32 SINGLE_SYMBOL_BIT = 0x80000000;
const U32 MAX_UNCODED_DATA_SIZE = 0x7FFFFFFF;
const U32 HUFFMAN_PARALLEL_STREAMS = 16;
#define HUFFMAN_16_STREAM_VECTORIZED
#define HUFFMAN_ASM_DECODER

Byte* encode(MemoryArena& arena, U32* encodedLen, Byte* data, U32 dataLen) {
	if (dataLen > MAX_UNCODED_DATA_SIZE) {
		printf("Tried to huffman compress too much data, shouldn't happen in DrillEngine use cases\n"a);
		return nullptr;
	}
	// Given that this is my first huffman encoder, I'm sure I'm doing a lot of things inefficiently here.
	// I don't care too much about encoding speed though, so it should be fine as long as the end huffman tree result is decent
	struct TreeNode {
		I32 symbol;
		I32 freq;
		I32 child0;
		I32 child1;
		I32 depth;
	};
	TreeNode nodes[512]{};
	U32 queue[512];
	for (U32 i = 0; i < 256; i++) {
		nodes[i].symbol = i;
		nodes[i].child0 = -1;
		nodes[i].child1 = -1;
		queue[i] = i;
	}

	// Build histogram
	for (U32 i = 0; i < dataLen; i++) {
		nodes[data[i]].freq++;
	}

	// Remove any non present symbols
	U32 queueSize = 0;
	for (U32 i = 0; i < 256; i++) {
		if (nodes[i].freq > 0) {
			queue[queueSize++] = i;
		}
	}

	// Early out for all same symbol (would break my huffman code, so we just special case it by sending the length and the symbol)
	if (queueSize <= 1) {
		Byte* result = arena.alloc<Byte>(5);
		STORE_LE32(result, SINGLE_SYMBOL_BIT | dataLen);
		result[4] = dataLen == 0 ? 0 : data[0];
		*encodedLen = 5;
		return result;
	}

	// Build unconstrained huffman tree. Could use a heap here, but whatever, only 256 items and compression doesn't need to be fast
	U32 maxInternalnode = 256;
	while (queueSize >= 2) {
		bool firstBetter = nodes[queue[0]].freq < nodes[queue[1]].freq;
		U32 minFreq0 = firstBetter ? queue[0] : queue[1];
		U32 minFreq1 = firstBetter ? queue[1] : queue[0];
		U32 queueIdx0 = firstBetter ? 0 : 1;
		U32 queueIdx1 = firstBetter ? 1 : 0;
		for (U32 i = 2; i < queueSize; i++) {
			I32 checkFreq = nodes[queue[i]].freq;
			if (checkFreq < nodes[minFreq1].freq) {
				minFreq1 = queue[i];
				queueIdx1 = i;
			}
			if (nodes[minFreq1].freq < nodes[minFreq0].freq) {
				swap(&minFreq0, &minFreq1);
				swap(&queueIdx0, &queueIdx1);
			}
		}
		U32 combined = maxInternalnode++;
		nodes[combined].freq = nodes[minFreq0].freq + nodes[minFreq1].freq;
		nodes[combined].child0 = minFreq0;
		nodes[combined].child1 = minFreq1;
		queue[queueIdx0] = combined;
		queue[queueIdx1] = queue[--queueSize];
	}

	// Get the depth of each item and count the number of depths we had to limit (this is a debt we will pay off later by demoting symbols)
	I32 debt = 0;
	U32 symbolDepthCounts[HUFFMAN_MAX_DEPTH]{};
	U32 queuePopPtr = 0;
	nodes[queue[0]].depth = 0;
	while (queuePopPtr != queueSize) {
		TreeNode& next = nodes[queue[queuePopPtr++]];
		if (next.child0 != -1) { // Internal node
			if (next.depth == HUFFMAN_MAX_DEPTH) {
				// Internal nodes at max depth are removed, since they would only lead into leaves that need to be clamped to max depth
				debt--;
			}
			nodes[next.child0].depth = next.depth + 1;
			nodes[next.child1].depth = next.depth + 1;
			queue[queueSize++] = next.child0;
			queue[queueSize++] = next.child1;
		} else { // Leaf node
			if (next.depth > HUFFMAN_MAX_DEPTH) {
				debt++;
				next.depth = HUFFMAN_MAX_DEPTH;
			}
			symbolDepthCounts[next.depth - 1]++;
		}
	}

	// Organize by depth and by frequency
	U32 symbolDepthOffsets[HUFFMAN_MAX_DEPTH]{};
	U32 totalActiveSymbols = 0;
	for (U32 i = 0; i < HUFFMAN_MAX_DEPTH; i++) {
		symbolDepthOffsets[i] = totalActiveSymbols;
		totalActiveSymbols += symbolDepthCounts[i];
	}
	U32 sortedSymbols[256];
	for (U32 i = 0; i < 256; i++) {
		if (nodes[i].freq) {
			sortedSymbols[symbolDepthOffsets[nodes[i].depth - 1]++] = i;
		}
	}
	for (U32 i = HUFFMAN_MAX_DEPTH - 1; i >= 1; i--) {
		symbolDepthOffsets[i] = symbolDepthOffsets[i - 1];
	}
	symbolDepthOffsets[0] = 0;
	for (U32 d = 0; d < HUFFMAN_MAX_DEPTH; d++) {
		U32 start = symbolDepthOffsets[d];
		U32 count = symbolDepthCounts[d];
		U32* toSort = sortedSymbols + start;
		for (U32 i = 0; i < count; i++) {
			U32 bestIdx = i;
			U32 bestFreq = nodes[toSort[bestIdx]].freq;
			for (U32 j = i + 1; j < count; j++) {
				U32 testFreq = nodes[toSort[j]].freq;
				if (testFreq > bestFreq) {
					bestIdx = j;
					bestFreq = testFreq;
				}
			}
			swap(&toSort[i], &toSort[bestIdx]);
		}
	}

	// Reclaim debt by demoting symbols. If the debt ends up negative (we had to demote something too big), promote symbols until it's zero
	// Idea from Yann Collet
	// https://fastcompression.blogspot.com/2015/07/huffman-revisited-part-3-depth-limited.html
	while (debt > 0) {
		U32 depthToDemote = HUFFMAN_MAX_DEPTH - (32 - lzcnt32(debt)) - 1;
		// Try to demote less frequent symbols first before we go up the tree and demote frequent ones
		for (U32 toDemote = depthToDemote; toDemote < HUFFMAN_MAX_DEPTH - 1; toDemote++) {
			if (symbolDepthCounts[toDemote] != 0) {
				depthToDemote = toDemote;
				break;
			}
		}
		while (symbolDepthCounts[depthToDemote] == 0) {
			depthToDemote--;
		}
		U32 demoteIdx = symbolDepthOffsets[depthToDemote] + symbolDepthCounts[depthToDemote] - 1;
		symbolDepthCounts[depthToDemote]--, symbolDepthCounts[depthToDemote + 1]++, symbolDepthOffsets[depthToDemote + 1]--;
		nodes[sortedSymbols[demoteIdx]].depth++;
		debt -= 1 << HUFFMAN_MAX_DEPTH - depthToDemote - 2;
	}
	while (debt < 0) {
		U32 depthToPromote = HUFFMAN_MAX_DEPTH - (31 - lzcnt32(-debt)) - 1;
		while (symbolDepthCounts[depthToPromote] == 0) {
			depthToPromote++;
		}
		U32 promoteIdx = symbolDepthOffsets[depthToPromote];
		symbolDepthCounts[depthToPromote]--, symbolDepthOffsets[depthToPromote]++, symbolDepthCounts[depthToPromote - 1]++;
		nodes[sortedSymbols[promoteIdx]].depth--;
		debt += 1 << HUFFMAN_MAX_DEPTH - depthToPromote - 1;
	}

	// Build a table of codes to write
	U32 startingCodes[HUFFMAN_MAX_DEPTH]{};
	for (U32 i = 0, currentCode = 0; i < HUFFMAN_MAX_DEPTH; i++) {
		startingCodes[i] = currentCode;
		currentCode += (1u << 31 - i) * symbolDepthCounts[i];
	}
	struct SymCode {
		U16 code;
		U16 len;
	} symCodes[256];
	U32 totalSymbolSlots = 0;
	for (U32 i = 0; i < 256; i++) {
		TreeNode& node = nodes[i];
		if (node.freq > 0) {
			totalSymbolSlots += 1 << HUFFMAN_MAX_DEPTH - node.depth;
			symCodes[i].len = node.depth;
			// We create the code MSB first because it's easier to work with, then write it LSB first because the decoder loop is faster that way
			symCodes[i].code = bitswap32(startingCodes[node.depth - 1]);
			startingCodes[node.depth - 1] += 1u << 32 - node.depth;
		} else {
			symCodes[i].len = 0;
			symCodes[i].code = 0;
		}
	}
	RUNTIME_ASSERT(totalSymbolSlots == 1 << HUFFMAN_MAX_DEPTH, "Huffman tree build failed\n"a);

	// Write out codes
	Byte* out = arena.stackBase + arena.stackPtr;
	Byte* result = out;
	STORE_LE32(out, dataLen);
	out += sizeof(U32);
	U64 streamBitLengths[HUFFMAN_PARALLEL_STREAMS]{};
	for (U32 i = 0; i < dataLen; i++) {
		SymCode code = symCodes[data[i]];
		streamBitLengths[i % HUFFMAN_PARALLEL_STREAMS] += code.len;
	}
	U32 nextStreamOffset = 0;
	U32 streamOffsets[HUFFMAN_PARALLEL_STREAMS];
	for (U32 i = 0; i < HUFFMAN_PARALLEL_STREAMS; i++) {
		U32 streamByteSize = (streamBitLengths[i] + 7) / 8 + 8;
		STORE_LE32(out, nextStreamOffset);
		out += sizeof(U32);
		streamOffsets[i] = nextStreamOffset;
		nextStreamOffset += streamByteSize;
	}
	for (U32 i = 0; i < 256; i += 2) {
		*out++ = symCodes[i].len | symCodes[i + 1].len << 4;
	}
	Byte* dataStreams[HUFFMAN_PARALLEL_STREAMS];
	for (U32 i = 0; i < HUFFMAN_PARALLEL_STREAMS; i++) {
		dataStreams[i] = out + streamOffsets[i];
	}
	U64 bitBufAll[HUFFMAN_PARALLEL_STREAMS]{};
	U32 bitBufBitsAll[HUFFMAN_PARALLEL_STREAMS]{};
	for (U32 i = 0; i < dataLen; i++) {
		U64& bitBuf = bitBufAll[i % HUFFMAN_PARALLEL_STREAMS];
		U32& bitBufBits = bitBufBitsAll[i % HUFFMAN_PARALLEL_STREAMS];
		Byte*& dataStream = dataStreams[i % HUFFMAN_PARALLEL_STREAMS];
		SymCode code = symCodes[data[i]];
		bitBuf |= U64(code.code) << bitBufBits;
		bitBufBits += code.len;
		if (bitBufBits > 64 - HUFFMAN_MAX_DEPTH) {
			STORE_LE64(dataStream, bitBuf);
			U32 bytesWritten = bitBufBits >> 3;
			dataStream += bytesWritten;
			if (bytesWritten == sizeof(U64)) {
				bitBuf = 0;
			} else {
				bitBuf >>= bitBufBits & ~0b111;
			}
			bitBufBits &= 0b111;
		}
	}
	for (U32 i = 0; i < HUFFMAN_PARALLEL_STREAMS; i++) {
		U64 bitBuf = bitBufAll[i];
		U32 bitBufBits = bitBufBitsAll[i];
		Byte* dataStream = dataStreams[i];
		if (bitBufBits) {
			STORE_LE64(dataStream, bitBuf);
		}
	}
	*encodedLen = U32(out + nextStreamOffset - result);
	arena.stackPtr += *encodedLen;
	return result;
}

DEBUG_OPTIMIZE_ON

Byte* decode(MemoryArena& arena, U32* decodedLen, Byte* data, U32 dataLen) {
	if (dataLen < sizeof(U32) + 1) {
		printf("Huffman stream too small, corrupted?\n"a);
		return nullptr;
	}
	F64 beginTime = current_time_seconds();
	U32 outputLen = LOAD_LE32(data);
	bool isSingleSymbol = outputLen & SINGLE_SYMBOL_BIT;
	outputLen &= ~SINGLE_SYMBOL_BIT;
	data += sizeof(U32), dataLen -= sizeof(U32);
	if (isSingleSymbol) {
		// Special case output for empty strings or single symbol strings
		Byte symbol = *data;
		Byte* out = arena.alloc<Byte>(outputLen);
		for (U32 i = 0; i < outputLen; i++) {
			out[i] = symbol;
		}
		*decodedLen = outputLen;
		return out;
	}
	if (dataLen < 128 + HUFFMAN_PARALLEL_STREAMS * sizeof(U32)) {
		printf("Huffman data stream did not have enough data for table\n"a);
		return nullptr;
	}
	U32 streamOffsets[HUFFMAN_PARALLEL_STREAMS];
	for (U32 i = 0; i < HUFFMAN_PARALLEL_STREAMS; i++) {
		streamOffsets[i] = LOAD_LE32(data);
		data += sizeof(U32), dataLen -= sizeof(U32);
	}
	struct HuffmanEntry {
		U8 len;
		U8 sym;
	};
	alignas(32) HuffmanEntry decodeTableReversed[1 << HUFFMAN_MAX_DEPTH];
	alignas(32) HuffmanEntry decodeTable[1 << HUFFMAN_MAX_DEPTH];
	U32 symbolDepthCounts[HUFFMAN_MAX_DEPTH + 4]{}; // + 4 to satisfy visual studio warning
	U32 totalSymbolSlots = 0;
	for (U32 i = 0; i < 256; i += 2) {
		U32 len0 = data[i >> 1] & 0xF;
		U32 len1 = data[i >> 1] >> 4;
		if (len0 > 0) {
			symbolDepthCounts[len0 - 1]++;
			totalSymbolSlots += 1 << HUFFMAN_MAX_DEPTH - len0;
		}
		if (len1 > 0) {
			symbolDepthCounts[len1 - 1]++;
			totalSymbolSlots += 1 << HUFFMAN_MAX_DEPTH - len1;
		}
	}
	if (totalSymbolSlots != 1 << HUFFMAN_MAX_DEPTH) {
		printf("Huffman table was invalid, should not happen\n"a);
		return nullptr;
	}
	U32 startingCodes[HUFFMAN_MAX_DEPTH + 3]; // + 3 to satisfy visual studio warning
	for (U32 i = 0, currentCode = 0; i < HUFFMAN_MAX_DEPTH; i++) {
		startingCodes[i] = currentCode;
		currentCode += (1 << HUFFMAN_MAX_DEPTH - 1 - i) * symbolDepthCounts[i];
	}
	// According to ryg's blog, it's faster to build the tree MSB first, then use a SIMD index bitswap to permute the table to LSB first. That keeps all the stores as linear fills.
	for (U32 sym = 0; sym < 256; sym++) {
		U32 len = data[sym >> 1] >> (sym & 1) * 4 & 0xF;
		if (len > 0) {
			U32 invDepth = HUFFMAN_MAX_DEPTH - len;
			HuffmanEntry* start = &decodeTableReversed[startingCodes[len - 1]];
			startingCodes[len - 1] += 1 << invDepth;
			U16 entry = len | sym << 8;
			switch (invDepth) {
			case 0: {
				STORE_LE16(start, entry);
			} break;
			case 1: {
				U32 entry2 = entry | entry << 16;
				STORE_LE32(start, entry2);
			} break;
			case 2: {
				U64 entry4 = U64(entry) * 0x0001000100010001ull;
				STORE_LE64(start, entry4);
			} break;
			case 3: {
				U64 entry4 = U64(entry) * 0x0001000100010001ull;
				STORE_LE64(start, entry4);
				STORE_LE64(start + 4, entry4);
			} break;
			default: {
				U32 size = 1 << invDepth;
				__m256i entry16 = _mm256_set1_epi16(entry);
				__m256i* storeTo = (__m256i*)start;
				for (U32 i = 0; i < size; i += 16) {
					_mm256_store_si256(storeTo++, entry16);
				}
			} break;
			}
		}
	}
	data += 128;
	dataLen -= 128;

	F64 tableBuildTime = current_time_seconds();

	{ // The table was built in MSB first order, so now we bit reverse it, since the data is in LSB first order
	  // Based on my bit reverse code for the FFT which performed significantly better than the non vectorized algorithm
		__m256i bitReverseLookup = _mm256_setr_epi8(0b0000, 0b1000, 0b0100, 0b1100, 0b0010, 0b1010, 0b0110, 0b1110, 0b0001, 0b1001, 0b0101, 0b1101, 0b0011, 0b1011, 0b0111, 0b1111, 0b0000, 0b1000, 0b0100, 0b1100, 0b0010, 0b1010, 0b0110, 0b1110, 0b0001, 0b1001, 0b0101, 0b1101, 0b0011, 0b1011, 0b0111, 0b1111);
		__m256i bitReverseLookupHi = _mm256_setr_epi8(0b00000000, 0b10000000, 0b01000000, 0b11000000, 0b00100000, 0b10100000, 0b01100000, 0b11100000, 0b00010000, 0b10010000, 0b01010000, 0b11010000, 0b00110000, 0b10110000, 0b01110000, 0b11110000, 0b00000000, 0b10000000, 0b01000000, 0b11000000, 0b00100000, 0b10100000, 0b01100000, 0b11100000, 0b00010000, 0b10010000, 0b01010000, 0b11010000, 0b00110000, 0b10110000, 0b01110000, 0b11110000);
		__m256i bswap16Shuffle = _mm256_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14, 1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
		__m256i lowBitMask = _mm256_set1_epi32(0x0F0F0F0F);
		__m256i indices = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
		__m256i sixteen = _mm256_set1_epi32(16);
		__m256i bitReversedEight = _mm256_set1_epi32(bitswap32(8) >> 32 - HUFFMAN_MAX_DEPTH);
		__m256i mask16 = _mm256_set1_epi32(0xFFFF);
		__m256i halfTable = _mm256_set1_epi32(1 << HUFFMAN_MAX_DEPTH - 1);
		__m256i* outputFirstHalf = (__m256i*) & decodeTable[0];
		__m256i* outputSecondHalf = (__m256i*) & decodeTable[1 << HUFFMAN_MAX_DEPTH - 1];
		for (U32 i = 0; i < 1 << HUFFMAN_MAX_DEPTH - 1; i += 16) {
			// This would be a little easier if I could support GF2P8AFFINEQB
			// x represents a hard 0 bit, number represents source bit index
			// nybbles xxxx|0123|4567|89ab -> xxxx|3210|xxxx|ba98
			__m256i permutedIndexBits1 = _mm256_shuffle_epi8(bitReverseLookup, _mm256_and_si256(indices, lowBitMask));
			// nybbles xxxx|0123|4567|89ab -> xxxx|xxxx|7654|xxxx
			__m256i permutedIndexBits2 = _mm256_shuffle_epi8(bitReverseLookupHi, _mm256_and_si256(_mm256_srli_epi16(indices, 4), lowBitMask));
			// nybbles xxxx|3210|xxxx|ba98 -> xxxx|ba98|xxxx|3210
			permutedIndexBits1 = _mm256_shuffle_epi8(permutedIndexBits1, bswap16Shuffle);
			__m256i permutedIndices = _mm256_srli_epi32(_mm256_or_si256(permutedIndexBits1, permutedIndexBits2), 1);
			__m256i gathered1 = _mm256_i32gather_epi32((int*)decodeTableReversed, permutedIndices, 2);
			__m256i gathered2 = _mm256_i32gather_epi32((int*)decodeTableReversed, _mm256_or_si256(permutedIndices, bitReversedEight), 2);
			__m256i lower = _mm256_permute4x64_epi64(_mm256_packus_epi32(_mm256_and_si256(gathered1, mask16), _mm256_and_si256(gathered2, mask16)), _MM_PERM_DBCA);
			__m256i upper = _mm256_permute4x64_epi64(_mm256_packus_epi32(_mm256_srli_epi32(gathered1, 16), _mm256_srli_epi32(gathered2, 16)), _MM_PERM_DBCA);
			_mm256_store_si256(outputFirstHalf++, lower);
			_mm256_store_si256(outputSecondHalf++, upper);
			indices = _mm256_add_epi32(indices, sixteen);
		}
	}

	F64 tableReverseTime = current_time_seconds();

	Byte* result = arena.alloc_aligned<Byte>(outputLen, 32);
	Byte* writePtr = result;

	static_assert(HUFFMAN_MAX_DEPTH <= 11, "Decoder loop is designed to work with 5 decodes per refill");

#ifdef HUFFMAN_ASM_DECODER
#ifdef HUFFMAN_16_STREAM_VECTORIZED
	{
		Huff16StreamDecodeCallArgs args{};
		args.decodeTable = (U16*)decodeTable;
		args.writePtr = writePtr;
		args.outputLen = outputLen;
		args.readPtr = data;
		for (U32 i = 0; i < 16; i++) {
			args.readOffset[i] = streamOffsets[i];
		}
		writePtr = huff_16_stream_decode(&args);
	}
#else
	{
		Huff4StreamDecodeCallArgs args{};
		args.decodeTable = (U16*)decodeTable;
		args.writePtr = writePtr;
		args.outputLen = outputLen;
		args.readPtr0 = data + streamOffsets[0];
		args.readPtr1 = data + streamOffsets[1];
		args.readPtr2 = data + streamOffsets[2];
		args.readPtr3 = data + streamOffsets[3];
		writePtr = huff_4_stream_decode(&args);
	}
#endif
#else
#ifdef HUFFMAN_16_STREAM_VECTORIZED
	{ // Decoder vector loop - even more heavily optimized!
		__m256i lookupMask = _mm256_set1_epi64x((1 << HUFFMAN_MAX_DEPTH) - 1);
		__m128i bitCountMask = _mm_set1_epi32(0xFF);
		__m128i shuffleData = _mm_setr_epi8(1, 5, 9, 13, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80);
		__m256i readPtr0to3 = _mm256_setr_epi64x(streamOffsets[0], streamOffsets[1], streamOffsets[2], streamOffsets[3]);
		__m256i readPtr4to7 = _mm256_setr_epi64x(streamOffsets[4], streamOffsets[5], streamOffsets[6], streamOffsets[7]);
		__m256i readPtr8to11 = _mm256_setr_epi64x(streamOffsets[8], streamOffsets[9], streamOffsets[10], streamOffsets[11]);
		__m256i readPtr12to15 = _mm256_setr_epi64x(streamOffsets[12], streamOffsets[13], streamOffsets[14], streamOffsets[15]);
		__m256i bitBuf0to3 = _mm256_i64gather_epi64((long long*)data, readPtr0to3, 1);
		__m256i bitBuf4to7 = _mm256_i64gather_epi64((long long*)data, readPtr4to7, 1);
		__m256i bitBuf8to11 = _mm256_i64gather_epi64((long long*)data, readPtr8to11, 1);
		__m256i bitBuf12to15 = _mm256_i64gather_epi64((long long*)data, readPtr12to15, 1);
		__m256i sixtyFour = _mm256_set1_epi64x(64);
		__m256i bitBufBits0to3 = sixtyFour;
		__m256i bitBufBits4to7 = sixtyFour;
		__m256i bitBufBits8to11 = sixtyFour;
		__m256i bitBufBits12to15 = sixtyFour;
		readPtr0to3 = _mm256_add_epi64(readPtr0to3, _mm256_set1_epi64x(sizeof(U64)));
		readPtr4to7 = _mm256_add_epi64(readPtr4to7, _mm256_set1_epi64x(sizeof(U64)));
		readPtr8to11 = _mm256_add_epi64(readPtr8to11, _mm256_set1_epi64x(sizeof(U64)));
		readPtr12to15 = _mm256_add_epi64(readPtr12to15, _mm256_set1_epi64x(sizeof(U64)));

		__m128i tableResult;
		__m256i bitCounts;
		__m256i nextBits;
		__m256i bytesLoaded;

#define EXTRACT_STEP(idx, bitBuf, bitBufBits)\
		tableResult = _mm256_i64gather_epi32((int*)decodeTable, _mm256_and_si256(bitBuf, lookupMask), 2);\
		bitCounts = _mm256_cvtepi32_epi64(_mm_and_si128(tableResult, bitCountMask));\
		bitBuf = _mm256_srlv_epi64(bitBuf, bitCounts);\
		bitBufBits = _mm256_sub_epi64(bitBufBits, bitCounts);\
		_mm_storeu_si32(writePtr + idx, _mm_shuffle_epi8(tableResult, shuffleData))

#define REFILL_STEP(readPtr, bitBuf, bitBufBits)\
		nextBits = _mm256_i64gather_epi64((long long*)data, readPtr, 1);\
		bitBuf = _mm256_or_si256(bitBuf, _mm256_sllv_epi64(nextBits, bitBufBits));\
		bytesLoaded = _mm256_srli_epi64(_mm256_sub_epi64(sixtyFour, bitBufBits), 3);\
		readPtr = _mm256_add_epi64(readPtr, bytesLoaded);\
		bitBufBits = _mm256_add_epi64(bitBufBits, _mm256_slli_epi64(bytesLoaded, 3))

		U32 bytesPerLoopIteration = 5 * HUFFMAN_PARALLEL_STREAMS;
		U32 toHandleInFastLoop = outputLen / bytesPerLoopIteration * bytesPerLoopIteration;
		U32 outputResidual = outputLen - toHandleInFastLoop;
		Byte* fastLoopEndPtr = writePtr + toHandleInFastLoop;
		while (writePtr != fastLoopEndPtr) {
			EXTRACT_STEP(0, bitBuf0to3, bitBufBits0to3);
			EXTRACT_STEP(4, bitBuf4to7, bitBufBits4to7);
			EXTRACT_STEP(8, bitBuf8to11, bitBufBits8to11);
			EXTRACT_STEP(12, bitBuf12to15, bitBufBits12to15);
			EXTRACT_STEP(16, bitBuf0to3, bitBufBits0to3);
			EXTRACT_STEP(20, bitBuf4to7, bitBufBits4to7);
			EXTRACT_STEP(24, bitBuf8to11, bitBufBits8to11);
			EXTRACT_STEP(28, bitBuf12to15, bitBufBits12to15);
			EXTRACT_STEP(32, bitBuf0to3, bitBufBits0to3);
			EXTRACT_STEP(36, bitBuf4to7, bitBufBits4to7);
			EXTRACT_STEP(40, bitBuf8to11, bitBufBits8to11);
			EXTRACT_STEP(44, bitBuf12to15, bitBufBits12to15);
			EXTRACT_STEP(48, bitBuf0to3, bitBufBits0to3);
			EXTRACT_STEP(52, bitBuf4to7, bitBufBits4to7);
			EXTRACT_STEP(56, bitBuf8to11, bitBufBits8to11);
			EXTRACT_STEP(60, bitBuf12to15, bitBufBits12to15);
			EXTRACT_STEP(64, bitBuf0to3, bitBufBits0to3);
			EXTRACT_STEP(68, bitBuf4to7, bitBufBits4to7);
			EXTRACT_STEP(72, bitBuf8to11, bitBufBits8to11);
			EXTRACT_STEP(76, bitBuf12to15, bitBufBits12to15);

			REFILL_STEP(readPtr0to3, bitBuf0to3, bitBufBits0to3);
			REFILL_STEP(readPtr4to7, bitBuf4to7, bitBufBits4to7);
			REFILL_STEP(readPtr8to11, bitBuf8to11, bitBufBits8to11);
			REFILL_STEP(readPtr12to15, bitBuf12to15, bitBufBits12to15);

			writePtr += bytesPerLoopIteration;
		}

		if (outputResidual == 0) goto finished;
		EXTRACT_STEP(0, bitBuf0to3, bitBufBits0to3);
		if (outputResidual < 4) goto finished;
		EXTRACT_STEP(4, bitBuf4to7, bitBufBits4to7);
		if (outputResidual < 8) goto finished;
		EXTRACT_STEP(8, bitBuf8to11, bitBufBits8to11);
		if (outputResidual < 12) goto finished;
		EXTRACT_STEP(12, bitBuf12to15, bitBufBits12to15);
		if (outputResidual < 16) goto finished;
		EXTRACT_STEP(16, bitBuf0to3, bitBufBits0to3);
		if (outputResidual < 20) goto finished;
		EXTRACT_STEP(20, bitBuf4to7, bitBufBits4to7);
		if (outputResidual < 24) goto finished;
		EXTRACT_STEP(24, bitBuf8to11, bitBufBits8to11);
		if (outputResidual < 28) goto finished;
		EXTRACT_STEP(28, bitBuf12to15, bitBufBits12to15);
		if (outputResidual < 32) goto finished;
		EXTRACT_STEP(32, bitBuf0to3, bitBufBits0to3);
		if (outputResidual < 36) goto finished;
		EXTRACT_STEP(36, bitBuf4to7, bitBufBits4to7);
		if (outputResidual < 40) goto finished;
		EXTRACT_STEP(40, bitBuf8to11, bitBufBits8to11);
		if (outputResidual < 44) goto finished;
		EXTRACT_STEP(44, bitBuf12to15, bitBufBits12to15);
		if (outputResidual < 48) goto finished;
		EXTRACT_STEP(48, bitBuf0to3, bitBufBits0to3);
		if (outputResidual < 52) goto finished;
		EXTRACT_STEP(52, bitBuf4to7, bitBufBits4to7);
		if (outputResidual < 56) goto finished;
		EXTRACT_STEP(56, bitBuf8to11, bitBufBits8to11);
		if (outputResidual < 60) goto finished;
		EXTRACT_STEP(60, bitBuf12to15, bitBufBits12to15);
		if (outputResidual < 64) goto finished;
		EXTRACT_STEP(64, bitBuf0to3, bitBufBits0to3);
		if (outputResidual < 68) goto finished;
		EXTRACT_STEP(68, bitBuf4to7, bitBufBits4to7);
		if (outputResidual < 72) goto finished;
		EXTRACT_STEP(72, bitBuf8to11, bitBufBits8to11);
		if (outputResidual < 76) goto finished;
		EXTRACT_STEP(76, bitBuf12to15, bitBufBits12to15);
	finished:;


		writePtr += outputResidual;
	}
#else
	{ // Decoder loop - heavily optimized!
		U16* decodeTable16 = (U16*)decodeTable;
		U16 tableResult = 0;
		// Control word used for bextr, bit pos in lower 8 bits, number of bits to extract in higher 8 bits
		U64 extractControl0 = HUFFMAN_MAX_DEPTH << 8;
		U64 extractControl1 = HUFFMAN_MAX_DEPTH << 8;
		U64 extractControl2 = HUFFMAN_MAX_DEPTH << 8;
		U64 extractControl3 = HUFFMAN_MAX_DEPTH << 8;
		Byte* readPtr0 = data + streamOffsets[0];
		Byte* readPtr1 = data + streamOffsets[1];
		Byte* readPtr2 = data + streamOffsets[2];
		Byte* readPtr3 = data + streamOffsets[3];
		U64 bitBuf0 = LOAD_LE64(readPtr0);
		U64 bitBuf1 = LOAD_LE64(readPtr1);
		U64 bitBuf2 = LOAD_LE64(readPtr2);
		U64 bitBuf3 = LOAD_LE64(readPtr3);
		readPtr0 += sizeof(U64);
		readPtr1 += sizeof(U64);
		readPtr2 += sizeof(U64);
		readPtr3 += sizeof(U64);
		//__m128i writeReg = _mm_setzero_si128();
		// Idea from Fabien Giesen's decoder series
#define EXTRACT_STEP(idx, bitBuf, extractControl)\
		tableResult = decodeTable16[_bextr2_u64(bitBuf, extractControl)];\
		extractControl += tableResult & 0xFF;\
		writePtr[idx] = tableResult >> 8
		//writeReg = _mm_insert_epi8(writeReg, tableResult >> 8, idx)
		
#define REFILL_STEP(bitBuf, readPtr, extractControl)\
		if ((extractControl & 0xFF) >= 8) {\
			bitBuf = _shrx_u64(bitBuf, extractControl & 0xF8);\
			bitBuf |= LOAD_LE64(readPtr) << 64 - (extractControl & 0xF8);\
			U32 bytesLoaded = (extractControl & 0xF8) >> 3;\
			extractControl &= 0xFF07;\
			readPtr += bytesLoaded;\
		}
		Byte* fastLoopEndPtr = writePtr + ALIGN_LOW(outputLen, 16);
		while (writePtr != fastLoopEndPtr) {
			EXTRACT_STEP(0, bitBuf0, extractControl0);
			EXTRACT_STEP(1, bitBuf1, extractControl1);
			EXTRACT_STEP(2, bitBuf2, extractControl2);
			EXTRACT_STEP(3, bitBuf3, extractControl3);
			EXTRACT_STEP(4, bitBuf0, extractControl0);
			EXTRACT_STEP(5, bitBuf1, extractControl1);
			EXTRACT_STEP(6, bitBuf2, extractControl2);
			EXTRACT_STEP(7, bitBuf3, extractControl3);
			EXTRACT_STEP(8, bitBuf0, extractControl0);
			EXTRACT_STEP(9, bitBuf1, extractControl1);
			EXTRACT_STEP(10, bitBuf2, extractControl2);
			EXTRACT_STEP(11, bitBuf3, extractControl3);
			EXTRACT_STEP(12, bitBuf0, extractControl0);
			EXTRACT_STEP(13, bitBuf1, extractControl1);
			EXTRACT_STEP(14, bitBuf2, extractControl2);
			EXTRACT_STEP(15, bitBuf3, extractControl3);
			REFILL_STEP(bitBuf0, readPtr0, extractControl0);
			REFILL_STEP(bitBuf1, readPtr1, extractControl1);
			REFILL_STEP(bitBuf2, readPtr2, extractControl2);
			REFILL_STEP(bitBuf3, readPtr3, extractControl3);

			//_mm_store_si128((__m128i*)writePtr, writeReg);
			writePtr += 16;
		}

		//writeReg = _mm_setzero_si128();

		U32 amountLeft = outputLen & 15;
		if (amountLeft == 0) goto finished;
		EXTRACT_STEP(0, bitBuf0, extractControl0);
		if (amountLeft == 1) goto finished;
		EXTRACT_STEP(1, bitBuf1, extractControl1);
		if (amountLeft == 2) goto finished;
		EXTRACT_STEP(2, bitBuf2, extractControl2);
		if (amountLeft == 3) goto finished;
		EXTRACT_STEP(3, bitBuf3, extractControl3);
		if (amountLeft == 4) goto finished;
		EXTRACT_STEP(4, bitBuf0, extractControl0);
		if (amountLeft == 5) goto finished;
		EXTRACT_STEP(5, bitBuf1, extractControl1);
		if (amountLeft == 6) goto finished;
		EXTRACT_STEP(6, bitBuf2, extractControl2);
		if (amountLeft == 7) goto finished;
		EXTRACT_STEP(7, bitBuf3, extractControl3);
		if (amountLeft == 8) goto finished;
		EXTRACT_STEP(8, bitBuf0, extractControl0);
		if (amountLeft == 9) goto finished;
		EXTRACT_STEP(9, bitBuf1, extractControl1);
		if (amountLeft == 10) goto finished;
		EXTRACT_STEP(10, bitBuf2, extractControl2);
		if (amountLeft == 11) goto finished;
		EXTRACT_STEP(11, bitBuf3, extractControl3);
		if (amountLeft == 12) goto finished;
		EXTRACT_STEP(12, bitBuf0, extractControl0);
		if (amountLeft == 13) goto finished;
		EXTRACT_STEP(13, bitBuf1, extractControl1);
		if (amountLeft == 14) goto finished;
		EXTRACT_STEP(14, bitBuf2, extractControl2);
	finished:;
		//_mm_store_si128((__m128i*)writePtr, writeReg);
		writePtr += outputLen & 15;
#undef EXTRACT_STEP
#undef REFILL_STEP
	}
#endif
#endif
	

	*decodedLen = writePtr - result;
	arena.stackPtr += *decodedLen;

	// Early tests indicate that the decode loop always takes at least 99% of the time for a 2k texture
	/* Original
	Table time: 0.000001600012183189392
	Reverse time: 0.0000004998873919248581
	Decode time: 0.007194299949333072
	Table: 0.022233508654082648%
	Reverse: 0.0069463537660528875%
	Decode: 99.97082013757986%

	Table time: 0.000001600012183189392
	Reverse time: 0.0000004998873919248581
	Decode time: 0.0007361001335084438
	Table: 0.21674507064242898%
	Reverse: 0.06771706441637006%
	Decode: 99.71553786494121%

	Table time: 0.0000014998950064182281
	Reverse time: 0.0000008998904377222061
	Decode time: 0.0007088000420480967
	Table: 0.2108964243856766%
	Reverse: 0.12653130708640795%
	Decode: 99.66257226852791%

	Table time: 0.000001700129359960556
	Reverse time: 0.000000400003045797348
	Decode time: 0.002213899977505207
	Table: 0.07672063518213743%
	Reverse: 0.01805067806668202%
	Decode: 99.90522868675117%
	*/
	/* Unrolled + intrinsics (roughly twice as fast huff decode)
	Table time: 0.0000016000012692529708
	Reverse time: 0.00000039999940781854093
	Decode time: 0.003639800001110416
	Table: 0.04393435302508786%
	Reverse: 0.010983563282503463%
	Decode: 99.9450820836924%

	Table time: 0.0000011000010999850929
	Reverse time: 0.00000039999940781854093
	Decode time: 0.00029940000240458176
	Table: 0.3655703188229566%
	Reverse: 0.13293433165403154%
	Decode: 99.50149534952301%

	Table time: 0.0000010000003385357559
	Reverse time: 0.0000002999986463692039
	Decode time: 0.0004914000019198284
	Table: 0.2029633319869046%
	Reverse: 0.060888704245651216%
	Decode: 99.73614796376744%

	Table time: 0.0000015000005078036338
	Reverse time: 0.0000002999986463692039
	Decode time: 0.0011149999991175719
	Table: 0.1343123665942778%
	Reverse: 0.026862343018754817%
	Decode: 99.83882529038696%
	*/
	F64 tableTime = tableBuildTime - beginTime;
	F64 reverseTime = tableReverseTime - tableBuildTime;
	F64 decodeTime = current_time_seconds() - tableReverseTime;
	//printf("Table time: %\nReverse time: %\nDecode time: %\n"a, tableTime, reverseTime, decodeTime);
	F64 total = tableTime + reverseTime + decodeTime;
	//printf("Table: %\\%\nReverse: %\\%\nDecode: %\\%\n"a, tableTime / total * 100.0, reverseTime / total * 100.0, decodeTime / total * 100.0);
	return result;
}

DEBUG_OPTIMIZE_OFF

}