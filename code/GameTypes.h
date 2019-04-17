#if !defined(GAMETYPES_H)

#include <stdint.h>

#define internal static
#define global_variable static
#define local_persist static

#define Pi32 3.14159265359

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

typedef intptr_t intptr;
typedef uintptr_t uintptr;

typedef uintptr_t umm;
typedef intptr_t smm;

#define GAMETYPES_H
#endif

