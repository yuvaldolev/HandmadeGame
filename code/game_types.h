#if !defined(GAMETYPES_H)

#include <stdint.h>

#define internal static
#define global_variable static
#define local_persist static

#define Pi32 3.14159265359f

/*
   NOTE(yuval): Build Options
GAME_INTERNAL:
0 - Build for public release
1 - Build for developers only

 GAME_SLOW:
 0 - No slow code is allowed
 1 - Slow code is welcome
*/

#if GAME_SLOW
#define Assert(expression) if (!(expression)) { *(int*)0 = 0; }
#else
#define Assert(expression)
#endif

// TODO(yuval & eran): Move this to another file
#define Kilobytes(value) (1024LL * (value))
#define Megabytes(value) (1024LL * Kilobytes(value))
#define Gigabytes(value) (1024LL * Megabytes(value))
#define Terabytes(value) (1024LL * Gigabytes(value))

// NOTE(yuval): To be used only in the same function
// where the array was defined!
#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef s32 b32;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef intptr_t smm;
typedef uintptr_t umm;

typedef size_t memory_index;

struct String
{
    char* data;
    umm count;
    memory_index memorySize;
};

inline u32
SafeTruncateToU32(u64 value)
{
    // TODO(yuval & eran): Defines for size limits
    Assert(value <= 0xFFFFFFFF);
    return (u32)value;
}

inline u32
StringLength(const char* string)
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
