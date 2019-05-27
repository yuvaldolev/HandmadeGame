#if !defined(GAME_SHARED_H)

#include <stdarg.h>

#define ReadVarArgUnsignedInteger(length, argList) (length == 8) ? \
va_arg(argList, u64) : va_arg(argList, u32)

#define ReadVarArgSignedInteger(length, argList) (length == 8) ? \
va_arg(argList, s64) : va_arg(argList, s32)

#define ReadVarArgFloat(length, argList) va_arg(argList, f64)

#define CopyArray(dest, source, count) Copy(dest, source, \
sizeof(*source) * count)

struct FormatDest
{
    char* at;
    umm size;
};

global_variable char globalDecChars[] = "0123456789";
global_variable char globalLowerHexChars[] = "0123456789abcdef";
global_variable char globalUpperHexChars[] = "0123456789ABCDEF";

internal void
ZeroSize(void* ptr, memory_index size)
{
    u8* byte = (u8*)ptr;
    
    while (size--)
    {
        *byte++ = 0;
    }
}

internal void*
Copy(void* destInit, const void* sourceInit, memory_index size)
{
    void* result = 0;
    
    if (destInit && sourceInit)
    {
        const u8* source = (const u8*)sourceInit;
        u8* dest = (u8*)destInit;
        
        while (size--)
        {
            *dest++ = *source++;
        }
        
        result = destInit;
    }
    
    return result;
}

internal char*
CopyZ(void* dest, const char* z, umm count)
{
    char* destInit = (char*)Copy(dest, z, count * sizeof(char));
    return destInit;
}

internal char*
CopyZ(void* dest, const char* z)
{
    return CopyZ(dest, z, StringLength(z));
}

internal String
MakeString(const char* z, void* memory, umm count, memory_index memorySize)
{
    // NOTE(yuval): z is REQUIRED to be null terminated!
    String str = { };
    
    if (memory && z)
    {
        Assert(count * sizeof(char) <= memorySize);
        
        str.data = CopyZ(memory, z, count);
        str.count = count;
        str.memorySize = memorySize;
    }
    
    return str;
}

internal String
MakeString(const char* z, void* memory, memory_index memorySize)
{
    // NOTE(yuval): z is REQUIRED to be null terminated!
    return MakeString(z, memory, StringLength(z), memorySize);
}

internal void
AppendZ(String* dest, const char* z, umm count)
{
    if (dest && z)
    {
        if (((dest->count + count) * sizeof(char)) <= dest->memorySize)
        {
            char* destAt = dest->data + dest->count;
            CopyZ(destAt, z, count);
            dest->count += count;
        }
    }
}

internal void
AppendZ(String* dest, const char* z)
{
    AppendZ(dest, z, StringLength(z));
}

internal void
AppendString(String* dest, const String* source)
{
    AppendZ(dest, source->data, source->count);
}

internal void
AdvanceString(String* value, umm count)
{
    if (value->count >= count)
    {
        value->data += count;
        value->count -= count;
        value->memorySize -= (count * sizeof(char));
    }
    else
    {
        value->data += value->count;
        value->count = 0;
        value->memorySize = 0;
    }
}

internal String
WrapZ(char* z)
{
    u32 zLength = StringLength(z);
    String str = { z, zLength, zLength * sizeof(char) };
    return str;
}

internal void
CatStrings(char* dest, memory_index destCount,
           const char* sourceA, memory_index sourceACount,
           const char* sourceB, memory_index sourceBCount)
{
    Assert(destCount > sourceACount + sourceBCount);
    char* destAt = dest;
    
    // TODO(yuval & eran): @Copy-and-paste
    // TODO(yuval & eran): @Incomplete use String struct
    for (s32 index = 0; index < sourceACount; ++index)
    {
        *destAt++ = sourceA[index];
    }
    
    for (s32 index = 0; index < sourceBCount; ++index)
    {
        *destAt++ = sourceB[index];
    }
    
    *destAt = 0;
}

internal char
ToLowercase(char value)
{
    char result = value;
    
    if ((result >= 'A') && (result <= 'Z'))
    {
        result += 'a' - 'A';
    }
    
    return result;
}

internal void
ToLowercase(char* value)
{
    while (*value)
    {
        *value++ = ToLowercase(*value);
    }
}

internal char
ToUppercase(char value)
{
    char result = value;
    
    if ((result >= 'a') && (result <= 'z'))
    {
        result -= 'a' - 'A';
    }
    
    return result;
}

internal void
ToUppercase(char* value)
{
    while (*value)
    {
        *value++ = ToUppercase(*value);
    }
}

internal void
OutChar(FormatDest* dest, char value)
{
    if (dest->size)
    {
        --dest->size;
        *dest->at++ = value;
    }
}

internal void
OutChars(FormatDest* dest, const char* value)
{
    // TODO(yuval & eran): Speed this up
    while (*value)
    {
        OutChar(dest, *value++);
    }
}

internal void
OutString(FormatDest* dest, String value)
{
    while (value.count--)
    {
        OutChar(dest, *value.data++);
    }
}

internal void
OutCharsLowercase(FormatDest* dest, const char* value)
{
    while (*value)
    {
        OutChar(dest, ToLowercase(*value++));
    }
}

internal void
OutCharsUppercase(FormatDest* dest, const char* value)
{
    while (*value)
    {
        OutChar(dest, ToUppercase(*value++));
    }
}

internal void
U64ToASCII(FormatDest* dest, u64 value, u32 base, const char* digits)
{
    Assert(base != 0);
    
    char* start = dest->at;
    
    do
    {
        u64 digitIndex = value % base;
        char digit = digits[digitIndex];
        OutChar(dest, digit);
        value /= base;
    }
    while (value != 0);
    
    char* end = dest->at;
    
    while (start < end)
    {
        --end;
        char temp = *end;
        *end = *start;
        *start = temp;
        ++start;
    }
}

internal void
F64ToASCII(FormatDest* dest, f64 value, u32 precision)
{
    if (value < 0)
    {
        OutChar(dest, '-');
        value = -value;
    }
    
    u64 integerPart = (u64)value;
    U64ToASCII(dest, integerPart, 10, globalDecChars);
    
    value -= integerPart;
    
    OutChar(dest, '.');
    
    // TODO(yuval & eran): Round to the precision
    for (u32 precisionIndex = 0;
         precisionIndex < precision;
         precisionIndex++)
    {
        value *= 10.0;
        
        u32 integer = (u32)value;
        OutChar(dest, globalDecChars[integer]);
        
        value -= integer;
    }
}

internal s32
S32FromZInternal(const char** atInit)
{
    const char* at = *atInit;
    s32 result = 0;
    
    while ((*at >= '0') && (*at <= '9'))
    {
        result *= 10;
        result += *at - '0';
        ++at;
    }
    
    *atInit = at;
    return result;
}

internal s32
S32FromZ(const char* at)
{
    const char* ignored = at;
    s32 result = S32FromZInternal(&ignored);
    return result;
}

internal umm
FormatStringList(char* destInit, umm destSize,
                 const char* format, va_list argList)
{
    FormatDest dest = { destInit, destSize };
    
    if (dest.size)
    {
        const char* formatAt = format;
        
        while (formatAt[0] && dest.size)
        {
            if (*formatAt == '%')
            {
                ++formatAt;
                
                // NOTE(yuval): Flag Handling
                bool parsing = true;
                bool forceSign = false;
                bool padWithZeros = false;
                bool leftJustify = false;
                bool positiveSignIsBlank = false;
                bool annotateIfNotZero = false;
                
                while (parsing)
                {
                    switch (*formatAt)
                    {
                        case '+': { forceSign = true; } break;
                        case '0': { padWithZeros = true; } break;
                        case '-': { leftJustify = true; } break;
                        case ' ': { positiveSignIsBlank = true; } break;
                        case '#': { annotateIfNotZero = true; } break;
                        default: { parsing = false; } break;
                    }
                    
                    if (parsing)
                    {
                        ++formatAt;
                    }
                }
                
                // NOTE(yuval): Width Handling
                b32 widthSpecified = false;
                s32 width = 0;
                
                if (*formatAt == '*')
                {
                    width = va_arg(argList, s32);
                    widthSpecified = true;
                    ++formatAt;
                }
                else if ((*formatAt >= '0') && (*formatAt <= '9'))
                {
                    width = S32FromZInternal(&formatAt);
                    widthSpecified = true;
                }
                
                // NOTE(yuval): Precision Handling
                b32 precisionSpecified = false;
                u32 precision = 0;
                
                if (*formatAt == '.')
                {
                    ++formatAt;
                    
                    if (*formatAt == '*')
                    {
                        precision = va_arg(argList, u32);
                        precisionSpecified = true;
                        ++formatAt;
                    }
                    else if ((*formatAt >= '0') && (*formatAt <= '9'))
                    {
                        precision = (u32)S32FromZInternal(&formatAt);
                        precisionSpecified = true;
                    }
                    else
                    {
                        Assert(!"Malformed Precision Specifier!");
                    }
                }
                
                // TODO(yuval & eran): Maybe replace this
                if (!precisionSpecified)
                {
                    precision = 6;
                }
                
                // NOTE(yuval): Length Handling
                u32 integerLength = 4;
                u32 floatLength = 8;
                
                // TODO(yuval & eran): Change length based on specified flags
                // For now this flags are disabled
                
                // NOTE(yuval): Regular Flags Handling
                char tempBuffer[64];
                char* temp = tempBuffer;
                FormatDest tempDest = { temp, ArrayCount(tempBuffer) };
                const char* prefix = "";
                b32 isFloat = false;
                
                switch (*formatAt)
                {
                    case 'd':
                    case 'i':
                    {
                        s64 value = ReadVarArgSignedInteger(integerLength, argList);
                        b32 wasNegative = value < 0;
                        
                        if (wasNegative)
                        {
                            value = -value;
                        }
                        
                        U64ToASCII(&tempDest, (u64)value, 10, globalDecChars);
                        
                        if (wasNegative)
                        {
                            prefix = "-";
                        }
                        else if (forceSign)
                        {
                            // TODO(yuval): Assert for positiveSignIsBlank
                            prefix = "+";
                        }
                        else if (positiveSignIsBlank)
                        {
                            prefix = " ";
                        }
                    } break;
                    
                    case 'u':
                    {
                        u64 value = ReadVarArgUnsignedInteger(integerLength, argList);
                        U64ToASCII(&tempDest, value, 10, globalDecChars);
                    } break;
                    
                    case 'o':
                    {
                        u64 value = ReadVarArgUnsignedInteger(integerLength, argList);
                        U64ToASCII(&tempDest, value, 8, globalDecChars);
                        
                        if (annotateIfNotZero && (value > 0))
                        {
                            prefix = "0";
                        }
                    } break;
                    
                    case 'x':
                    {
                        u64 value = ReadVarArgUnsignedInteger(integerLength, argList);
                        U64ToASCII(&tempDest, value, 16, globalLowerHexChars);
                        
                        if (annotateIfNotZero && (value > 0))
                        {
                            prefix = "0x";
                        }
                    } break;
                    
                    case 'X':
                    {
                        u64 value = ReadVarArgUnsignedInteger(integerLength, argList);
                        U64ToASCII(&tempDest, value, 16, globalUpperHexChars);
                        
                        if (annotateIfNotZero && (value > 0))
                        {
                            prefix = "0X";
                        }
                    } break;
                    
                    // TODO(yuval & eran): Different handling for different float types
                    case 'f':
                    case 'F':
                    case 'e':
                    case 'E':
                    case 'g':
                    case 'G':
                    case 'a':
                    case 'A':
                    {
                        f64 value = ReadVarArgFloat(floatLength, argList);
                        F64ToASCII(&tempDest, value, precision);
                        isFloat = true;
                    } break;
                    
                    case 'c':
                    {
                        s32 value = va_arg(argList, s32);
                        OutChar(&tempDest, (char)value);
                    } break;
                    
                    case 's':
                    {
                        char* value = va_arg(argList, char*);
                        temp = value;
                        
                        if (precisionSpecified)
                        {
                            tempDest.size = 0;
                            
                            for (char* scan = value;
                                 *scan && (tempDest.size < precision);
                                 ++scan)
                            {
                                ++tempDest.size;
                            }
                        }
                        else
                        {
                            tempDest.size = StringLength(value);
                        }
                        
                        tempDest.at = value + tempDest.size;
                    } break;
                    
                    /*
                    case 'S':
                    {
                    
                    } break;*/
                    
                    case 'p':
                    {
                        void* value = va_arg(argList, void*);
                        U64ToASCII(&tempDest, *(umm*)&value, 16, globalLowerHexChars);
                    } break;
                    
                    case 'n':
                    {
                        s32* tabDest = va_arg(argList, s32*);
                        *tabDest = (int)(dest.at - destInit);
                    } break;
                    
                    case '%':
                    {
                        OutChar(&tempDest, '%');
                    } break;
                    
                    default:
                    {
                        Assert(!"Unrecognized Format Specifier!");
                    } break;
                }
                
                if (tempDest.at - temp)
                {
                    smm usePrecision = precision;
                    if (isFloat || !precisionSpecified)
                    {
                        usePrecision = tempDest.at - temp;
                    }
                    
                    smm prefixLength = StringLength(prefix);
                    smm useWidth = width;
                    smm computedWidth = usePrecision + prefixLength;
                    
                    if (useWidth < computedWidth)
                    {
                        useWidth = computedWidth;
                    }
                    
                    if (padWithZeros)
                    {
                        // TODO(yuval & eran): Assert for left justify
                        leftJustify = false;
                    }
                    
                    if (!leftJustify)
                    {
                        while (useWidth > computedWidth)
                        {
                            OutChar(&dest, padWithZeros ? '0' : ' ');
                            --useWidth;
                        }
                    }
                    
                    for (const char* pre = prefix; *pre && useWidth; ++pre)
                    {
                        OutChar(&dest, *pre);
                        --useWidth;
                    }
                    
                    if (usePrecision > useWidth)
                    {
                        usePrecision = useWidth;
                    }
                    
                    while (usePrecision > (tempDest.at - temp))
                    {
                        OutChar(&dest, '0');
                        --usePrecision;
                        --useWidth;
                    }
                    
                    while (usePrecision && (tempDest.at - temp))
                    {
                        OutChar(&dest, *temp++);
                        --usePrecision;
                        --useWidth;
                    }
                    
                    if (leftJustify)
                    {
                        while (useWidth)
                        {
                            OutChar(&dest, ' ');
                            --useWidth;
                        }
                    }
                }
                
                if (*formatAt)
                {
                    ++formatAt;
                }
            }
            else
            {
                OutChar(&dest, *formatAt++);
            }
        }
        
        if (dest.size)
        {
            dest.at[0] = 0;
        }
        else
        {
            dest.at[-1] = 0;
        }
    }
    
    umm result = dest.at - destInit;
    return result;
}

internal umm
FormatString(char* dest, umm destSize, const char* format, ...)
{
    va_list argList;
    
    va_start(argList, format);
    umm result = FormatStringList(dest, destSize, format, argList);
    va_end(argList);
    
    return result;
}

#define GAME_SHARED_H
#endif

