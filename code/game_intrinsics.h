#if !defined(GAME_INTRINSICS_H)

// TODO(yuval, eran): Convert all of these to platform-efficient versions
// and remove math.h

#include <math.h>

inline s32
RoundF32ToS32(f32 Value)
{
    s32 Result = (s32)roundf(Value);
    return Result;
}

inline u32
RoundF32ToU32(f32 Value)
{
    u32 Result = (u32)roundf(Value);
    return Result;
}

inline s32
FloorF32ToS32(f32 Value)
{
    s32 Result = (s32)floorf(Value);
    return Result;
}

inline s32
TruncateF32ToS32(f32 Value)
{
    s32 Result = (s32)Value;
    return Result;
}

inline u32
SafeTruncateToU32(u64 Value)
{
    // TODO(yuval & eran): Defines for size limits
    Assert(Value <= 0xFFFFFFFF);
    return (u32)Value;
}

inline f32
Sin(f32 Angle)
{
    f32 Result = sinf(Angle);
    return Result;
}

inline f32
Cos(f32 Angle)
{
    f32 Result = cosf(Angle);
    return Result;
}

inline f32
ATan2(f32 Y, f32 X)
{
    f32 Result = atan2f(Y, X);
    return Result;
}

struct bit_scan_result
{
    s32 Index;
    b32 Found;
};

inline bit_scan_result
FindLeastSignificantSetBit(u32 Value)
{
    bit_scan_result Result = { };
    
#if COMPILER_MSVC
    Result.Found = _BitScanForward((unsigned long*)&Result.Index, Value);
#else
    for (u32 Index = 0; Index < 32; ++Index)
    {
        if (Value & (1 << Index))
        {
            Result.Index = Index;
            Result.Found = true;
            break;
        }
    }
#endif
    
    return Result;
}

#define GAME_INTRINSICS_H
#endif