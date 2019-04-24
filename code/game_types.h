#if !defined(GAMETYPES_H)

#include <stdint.h>

#define internal static
#define global_variable static
#define local_persist static

#define Pi32 3.14159265359

// NOTE(yuval): To be used only in the same function
// where the array was defined!
#define ArrayCount(array) (sizeof(array) / sizeof(array[0]))

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef s32 b32;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float r32;
typedef double r64;

typedef intptr_t smm;
typedef uintptr_t umm;

typedef size_t memory_index;

inline u32
StringLength(char* string)
{
    u32 count = 0;

    if (string)
    {
        while(*string++)
        {
            ++count;
        }
    }

    return count;
}

#define GAMETYPES_H
#endif

