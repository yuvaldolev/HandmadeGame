#if !defined(GAMESHARED_H)

#include <stdarg.h>

#define ReadVarArgUnsignedInteger(length, argList) (length == 8) ? \
    va_arg(argList, u64) : va_arg(argList, u32)

#define ReadVarArgSignedInteger(length, argList) (length == 8) ? \
    va_arg(argList, s64) : va_arg(argList, s32)

#define ReadVarArgFloat(length, argList) va_arg(argList, r64)

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
Copy(void* sourceInit, void* destInit, memory_index size)
{
    u8* source = (u8*)sourceInit;
    u8* dest = (u8*)destInit;

    while (size--)
    {
        *dest++ = *source++;
    }

    return destInit;
}

#define CopyArray(source, dest, count) Copy(source, dest, \
    sizeof(*source) * count)

internal void
OutChar(FormatDest* dest, char value)
{
    if (dest->size)
    {
        --dest->size;
        *dest->at++ = value;
    }
}

internal void OutChars(FormatDest* dest, char* value)
{
    // TODO(yuval & eran): Speed this up
    while (*value)
    {
        OutChar(dest, *value++);
    }
}

internal void
U64ToASCII(FormatDest* dest, u64 value, u32 base, const char* digits)
{
    // TODO(yuval & eran): Assert that base is not 0

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
F64ToASCII(FormatDest* dest, r64 value, u32 precision)
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

    for (u32 precisionIndex = 0;
         precisionIndex < precision;
         precisionIndex++)
    {
        value *= 10.0f;

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
                s32 precision = 0;

                if (*formatAt == '.')
                {
                    ++formatAt;

                    if (*formatAt == '*')
                    {
                        precision = va_arg(argList, s32);
                        precisionSpecified = true;
                        ++formatAt;
                    }
                    else if ((*formatAt >= '0') && (*formatAt <= '9'))
                    {
                        S32FromZInternal(&formatAt);
                        precisionSpecified = true;
                    }
                    else
                    {
                        // TODO(yuval & eran): Assert
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
                char* prefix = "";
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
                        r64 value = ReadVarArgFloat(floatLength, argList);
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
                        // TODO(yuval & eran): Assert
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
FormatString(char* dest, umm destSize, char* format, ...)
{
    va_list argList;

    va_start(argList, format);
    umm result = FormatStringList(dest, destSize, format, argList);
    va_end(argList);

    return result;
}

#define GAMESHARED_H
#endif

