#if !defined(GAME_INTRINSICS_H)

// TODO(yuval, eran): Convert all of these to platform-efficient versions
// and remove math.h

#include <math.h>

inline s32
RoundF32ToS32(f32 value)
{
    s32 result = (s32)roundf(value);
    return result;
}

inline u32
RoundF32ToU32(f32 value)
{
    u32 result = (u32)roundf(value);
    return result;
}

inline s32
FloorF32ToS32(f32 value)
{
    s32 result = (s32)floorf(value);
    return result;
}

inline s32
TruncateF32ToS32(f32 value)
{
    s32 result = (s32)value;
    return result;
}

inline u32
SafeTruncateToU32(u64 value)
{
    // TODO(yuval & eran): Defines for size limits
    Assert(value <= 0xFFFFFFFF);
    return (u32)value;
}

inline f32
Sin(f32 angle)
{
    f32 result = sinf(angle);
    return result;
}

inline f32
Cos(f32 angle)
{
    f32 result = cosf(angle);
    return result;
}

inline f32
ATan2(f32 Y, f32 X)
{
    f32 result = atan2f(Y, X);
    return result;
}

struct BitScanResult
{
    s32 index;
    b32 found;
};

inline BitScanResult
FindLeastSignificantSetBit(u32 value)
{
    BitScanResult result = { };
#if COMPILER_MSVC
    result.found = _BitScanForward((unsigned long*)&result.index, value);
#else
    for (u32 index = 0; index < 32; ++index)
    {
        if (value & (1 << index))
        {
            result.index = index;
            result.found = true;
            break;
        }
    }
#endif
    return result;
}

#define GAME_INTRINSICS_H
#endif