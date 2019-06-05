#if !defined(GAME_TYPES_H)

//////////////////////////////
//        Compilers         //
//////////////////////////////
#if !defined(COMPILER_MSVC)
#define COMPILER_MSVC 0
#endif

#if !defined(COMPILER_LLVM)
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM
#if defined(_MSC_VER)
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
// TODO(yuval, eran): Support More Compilers!
#undef COMPILER_LLVM
#define COMPILER_LLVM 1
#endif
#endif

#if COMPILER_MSVC
#include <intrin.h>
#elif COMPILER_LLVM
#include <x86intrin.h>
#else
#error Optimizations are not available for this compiler yet!!!
#endif

//////////////////////////////
//          Types           //
//////////////////////////////
#include <stdint.h>
#include <stddef.h>

#define internal static
#define global_variable static
#define local_persist static

#define Pi32 3.14159265359f

// TODO(yuval & eran): Move this to another file
#define Kilobytes(value) (1024LL * (value))
#define Megabytes(value) (1024LL * Kilobytes(value))
#define Gigabytes(value) (1024LL * Megabytes(value))
#define Terabytes(value) (1024LL * Gigabytes(value))

// NOTE(yuval): To be used only in the same function
// where the array was defined!
#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))

// NOTE(yuval): Array Foreach
#define ArrayForeach(var, array) \
for (s32 arrayIndex = 0; arrayIndex < ArrayCount(array); ++arrayIndex) \
for (b32 b = true; b; b = false) \
for (var = (array)[arrayIndex]; b; b = false)

#define ArrayForeachRef(var, array) \
for (s32 arrayIndex = 0; arrayIndex < ArrayCount(array); ++arrayIndex) \
for (b32 b = true; b; b = false) \
for (var = &(array)[arrayIndex]; b; b = false)

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

struct string
{
    char* Data;
    umm Count;
    memory_index MemorySize;
};

union v2
{
    struct
    {
        f32 X, Y;
    };
    
    f32 E[2];
};

inline u32
StringLength(const char* String)
{
    u32 Count = 0;
    
    if (String)
    {
        while(*String++)
        {
            ++Count;
        }
    }
    
    return Count;
}

#define GAME_TYPES_H
#endif

