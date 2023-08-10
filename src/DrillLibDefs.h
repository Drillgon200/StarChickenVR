#pragma once

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#define FINLINE __forceinline
#define NODISCARD [[nodiscard]]

#define ONE_KILOBYTE 1024
#define ONE_MEGABYTE (ONE_KILOBYTE * 1024)
#define ONE_GIGABYTE (ONE_MEGABYTE * 1024)

// Standard 4k page
#define PAGE_SIZE 4096

#define ARRAY_COUNT(arr) (sizeof(arr)/sizeof(arr[0]))
#define ALIGN_LOW(num, alignment) ((num) & ~((alignment) - 1))
#define ALIGN_HIGH(num, alignment) (((num) + ((alignment) - 1)) & ~((alignment) - 1))
// Undefined behavior for sure, but reimplementing many parts of the standard library pretty much requires that
#define OFFSET_OF(type, member) __builtin_offsetof(type, member) //(reinterpret_cast<uptr>(&reinterpret_cast<type*>(uptr(0))->member))

#define MATH_PI 3.1415926535F
#define DEG_TO_RAD(x) ((x)*(MATH_PI/180.0F))
#define RAD_TO_DEG(x) ((x)*(180.0F/MATH_PI))
#define TURN_TO_RAD(x) ((x)*(2.0F*MATH_PI))
#define RAD_TO_TURN(x) ((x)*(1.0F/(2.0F*MATH_PI)))

#define DRILL_LIB_MAKE_VERSION(major, minor, patch) ((((major) & 0b1111111111) << 20) | (((minor) & 0b1111111111) << 10) | ((patch) & 0b1111111111))

typedef signed __int8 i8;
typedef unsigned __int8 u8;
typedef signed __int16 i16;
typedef unsigned __int16 u16;
typedef signed __int32 i32;
typedef unsigned __int32 u32;
typedef signed __int64 i64;
typedef unsigned __int64 u64;
typedef u64 uptr;
typedef u8 byte;
typedef float f32;
typedef double f64;
typedef u32 b32;
typedef u32 flags32;

#define U32_MAX 0xFFFFFFFF
#define U64_MAX 0xFFFFFFFFFFFFFFFFULL
#define I32_MAX 0x7FFFFFFF
#define I64_MAX 0x7FFFFFFFFFFFFFFFULL

#define DRILL_LIB_REDECLARE_STDINT
#ifdef DRILL_LIB_REDECLARE_STDINT
typedef i8 int8_t;
typedef u8 uint8_t;
typedef i16 int16_t;
typedef u16 uint16_t;
typedef i32 int32_t;
typedef u32 uint32_t;
typedef i64 int64_t;
typedef u64 uint64_t;
typedef uptr uintptr_t;
#endif

void print(const char* str);
void print_integer(u64 num);
void print_float(f32 f);
void println_integer(u64 num);
void println_float(f32 f);
void abort(const char* message);