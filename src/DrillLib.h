#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include "DrillLibDefs.h"
#include "DrillMath.h"

extern "C" int _fltused = 0;

void zero_memory(void* mem, u32 bytes) {
	u8* byteMem = reinterpret_cast<u8*>(mem);
	while (bytes--) {
		*byteMem++ = 0;
	}
}

// For some reason these functions fail to link in release mode but not in debug mode?
#ifdef NDEBUG

#pragma intrinsic(memcpy, memset, strcmp, strlen, memcmp)

#pragma function(memcpy)
FINLINE void* __cdecl memcpy(void* dst, const void* src, size_t count) {
	const byte* bSrc = reinterpret_cast<const byte*>(src);
	byte* bDst = reinterpret_cast<byte*>(dst);
	/*while (count--) {
		*bDst++ = *bSrc++;
	}*/
	__movsb(bDst, bSrc, count);
	return dst;
}

#pragma function(memset)
void* __cdecl memset(void* dst, int val, size_t count) {
	byte* bDst = reinterpret_cast<byte*>(dst);
	/*while (count--) {
		*bDst++ = val;
	}*/
	__stosb(bDst, byte(val), count);
	return dst;
}

#pragma function(strcmp)
int __cdecl strcmp(const char* s1, const char* s2) {
	const byte* bS1 = reinterpret_cast<const byte*>(s1);
	const byte* bS2 = reinterpret_cast<const byte*>(s2);
	byte b1 = 0;
	byte b2 = 0;
	while ((b1 = *bS1++) == (b2 = *bS2++)) {
		if (b1 == '\0') {
			break;
		}
	}
	return b1 - b2;
}

#pragma function(strlen)
size_t __cdecl strlen(const char* str) {
	size_t result = 0;
	if (str != nullptr) {
		while (*str++) {
			result++;
		}
	}
	return result;
}

#pragma function(memcmp)
int __cdecl memcmp(const void* m1, const void* m2, size_t n) {
	int diff = 0;
	const byte* bM1 = reinterpret_cast<const byte*>(m1);
	const byte* bM2 = reinterpret_cast<const byte*>(m2);
	while (n--) {
		if (*bM1++ != *bM2++) {
			diff = *--bM1 - *--bM2;
			break;
		}
	}
	return diff;
}

#endif

template<typename T>
FINLINE T max(T a, T b) {
	return a > b ? a : b;
}

template<typename T>
FINLINE T min(T a, T b) {
	return a < b ? a : b;
}

struct MemoryArena {
	u8* stackBase;
	u64 stackMaxSize;
	u32 stackPtr;
	u32 frameBase;

	bool init(u64 capacity) {
		stackBase = reinterpret_cast<u8*>(VirtualAlloc(nullptr, (capacity + 4095) & ~0xFFF, MEM_RESERVE, PAGE_READWRITE));
		if (!stackBase) {
			return false;
		}
		stackMaxSize = capacity;
		return true;
	}

	void destroy() {
		VirtualFree(stackBase, 0, MEM_RELEASE);
	}

	template<typename T>
	T* realloc(T* data, u32 oldCount, u32 newCount) {
		T* result = nullptr;
		if (data != nullptr && reinterpret_cast<uptr>(data + oldCount) == reinterpret_cast<uptr>(stackBase + stackPtr)) {
			stackPtr += newCount - oldCount;
			result = data;
		} else {
			if (newCount > oldCount) {
				stackPtr = ALIGN_HIGH(stackPtr, alignof(T));
				result = reinterpret_cast<T*>(stackBase + stackPtr);
				if (data) {
					memcpy(result, data, newCount * sizeof(T));
				}
				stackPtr += newCount * sizeof(T);
			}
		}
		return result;
	}

	template<typename T>
	T* alloc(u32 count) {
		stackPtr = ALIGN_HIGH(stackPtr, alignof(T));
		T* result = reinterpret_cast<T*>(stackBase + stackPtr);
		stackPtr += count * sizeof(T);
		return result;
	}

	void push_frame() {
		u32 oldFrameBase = frameBase;
		frameBase = stackPtr;
		stackPtr = ALIGN_HIGH(stackPtr, alignof(u32));
		*reinterpret_cast<u32*>(stackBase + stackPtr) = oldFrameBase;
		stackPtr += sizeof(frameBase);
	}

	void pop_frame() {
		stackPtr = frameBase;
		u32 oldFrameBaseOffset = ALIGN_HIGH(stackPtr, alignof(u32));
		frameBase = *reinterpret_cast<u32*>(stackBase + oldFrameBaseOffset);
	}
};

// Things allocated here get pushed and popped with the call stack, 
// importantly, not necessarily at the same time as scopes, allowing objects to be passed out of their C++ scope
MemoryArena stackArena{};
// Things allocated here exist for one frame. It is reset at the beginning of each frame
MemoryArena frameArena{};
// Things allocated here exist for the duration of the program, it is never reset
MemoryArena globalArena{};

template<typename T, MemoryArena* allocator = &stackArena>
struct ArenaArrayList {
	T* data;
	u32 size;
	u32 capacity;

	FINLINE void reserve(u32 newCapacity) {
		if (newCapacity > capacity) {
			data = allocator->realloc(data, capacity, newCapacity);
			capacity = newCapacity;
		}
	}

	FINLINE void resize(u32 newSize) {
		if (newSize < size) {
			size = newSize;
		} else if (newSize > size) {
			reserve(newSize);
			zero_memory(data + size, (newSize - size) * sizeof(T));
		}
	}

	FINLINE void push_back(const T value) {
		if (size == capacity) {
			reserve(max<u32>(capacity * 2, 8));
		}
		data[size++] = value;
	}

	FINLINE void push_back_n(const T* values, u32 numValues) {
		u32 newCapacity = capacity;
		if (newCapacity == 0) {
			newCapacity = 8;
		}
		u32 newSize = size + numValues;
		while (newCapacity < newSize) {
			newCapacity *= 2;
		}
		reserve(newCapacity);
		T* dst = data + size;
		while (numValues--) {
			*dst++ = *values++;
		}
		size = newSize;
	}

	FINLINE bool contains(const T& value) {
		T* begin = data;
		T* end = begin + size;
		bool returnVal = false;
		while (begin != end) {
			if (*begin == value) {
				returnVal = true;
				break;
			}
			begin++;
		}
		return returnVal;
	}

	FINLINE bool subrange_contains(u32 rangeStart, u32 rangeEnd, const T& value) {
		bool returnVal = false;
		rangeStart = min(rangeStart, size);
		rangeEnd = min(rangeEnd, size);
		if (rangeStart >= rangeEnd) {
			goto end;
		}
		T* begin = data + rangeStart;
		T* end = begin + rangeEnd;
		
		while (begin != end) {
			if (*begin == value) {
				returnVal = true;
				break;
			}
			begin++;
		}
		end:;
		return returnVal;
	}
};

void (*previousPageFaultHandler)(int);

// This handler is used so we can reserve large amounts of memory up front and only commit it when we need to
LONG WINAPI page_fault_handler(PEXCEPTION_POINTERS exceptionPointers) {
	LONG result = EXCEPTION_CONTINUE_SEARCH;
	if (exceptionPointers->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
		// https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-exception_record
		// For EXCEPTION_ACCESS_VIOLATION
		// ExceptionInformation[0] contains 0 if read, 1 if write
		// ExceptionInformation[1] contains the virtual address accessed
		uptr address = exceptionPointers->ExceptionRecord->ExceptionInformation[1];
		LPVOID allocatedAddress = nullptr;
		if ((address >= reinterpret_cast<uptr>(stackArena.stackBase) && (address - reinterpret_cast<uptr>(stackArena.stackBase)) < stackArena.stackMaxSize) ||
			(address >= reinterpret_cast<uptr>(frameArena.stackBase) && (address - reinterpret_cast<uptr>(frameArena.stackBase)) < frameArena.stackMaxSize) ||
			(address >= reinterpret_cast<uptr>(globalArena.stackBase) && (address - reinterpret_cast<uptr>(globalArena.stackBase)) < globalArena.stackMaxSize)) {
			allocatedAddress = VirtualAlloc(reinterpret_cast<void*>(address), PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);
		}
		if (allocatedAddress) {
			result = EXCEPTION_CONTINUE_EXECUTION;
		}
	}
	return result;
}

HFILE consoleOutput;

void print(const char* str) {
	DWORD bytesWritten;
	WriteFile(reinterpret_cast<HANDLE>(consoleOutput), str, strlen(str), &bytesWritten, nullptr);
}

void println() {
	print("\n");
}

void println(const char* str) {
	print(str);
	print("\n");
}

void print_integer(u64 num) {
	if (num == 0) {
		print("0");
	} else {
		char buffer[32];
		buffer[31] = '\0';
		char* str = buffer + 31;
		while (num) {
			*--str = (num % 10) + '0';
			num /= 10;
		}
		print(str);
	}
}

void print_integer_pad(u64 num, u32 pad) {
	char buffer[32];
	buffer[31] = '\0';
	char* str = buffer + 31;
	while (num) {
		*--str = (num % 10) + '0';
		num /= 10;
		pad--;
	}
	while (pad--) {
		*--str = '0';
	}
	print(str);
}

void print_float(f32 f) {
	if (f < 0.0F) {
		print("-");
		f = -f;
	}
	u32 wholePart = u32(truncf(f));
	u32 fractionalPart = u32(fractf(f) * 10000.0F);
	print_integer(wholePart);
	print(".");
	print_integer_pad(fractionalPart, 4);
}

void println_integer(u64 num) {
	print_integer(num);
	print("\n");
}

void println_float(f32 f) {
	print_float(f);
	print("\n");
}

void abort(const char* message) {
	print(message);
	__debugbreak();
	ExitProcess(EXIT_FAILURE);
}

template<typename T>
T* read_full_file_to_arena(u32* count, MemoryArena& arena, const char* fileName) {
	T* result = nullptr;
	OFSTRUCT ofstruct{};
	HANDLE file = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (file != INVALID_HANDLE_VALUE) {
		u32 size = GetFileSize(file, NULL);
		result = arena.alloc<T>(size);
		DWORD numBytesRead{};
		if (!ReadFile(file, result, size, &numBytesRead, NULL)) {
			result = nullptr;
		}
		CloseHandle(file);
		*count = numBytesRead / sizeof(T);
	}
	return result;
}

bool drill_lib_init() {
	OFSTRUCT fstr{};
	consoleOutput = OpenFile("CON", &fstr, OF_WRITE);
	AddVectoredExceptionHandler(1, page_fault_handler);
	bool returnSuccess = false;
	if (!stackArena.init(ONE_GIGABYTE)) {
		goto end;
	}
	if (!frameArena.init(ONE_GIGABYTE)) {
		goto end;
	}
	if (!globalArena.init(ONE_GIGABYTE)) {
		goto end;
	}
	returnSuccess = true;
end:;
	return returnSuccess;
}