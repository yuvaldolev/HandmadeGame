#if !defined(GAME_SHARED_H)

#include <stdarg.h>

#define ReadVarArgUnsignedInteger(Length, ArgList) (Length == 8) ? \
va_arg(ArgList, u64) : va_arg(ArgList, u32)

#define ReadVarArgSignedInteger(Length, ArgList) (Length == 8) ? \
va_arg(ArgList, s64) : va_arg(ArgList, s32)

#define ReadVarArgFloat(Length, ArgList) va_arg(ArgList, f64)

#define CopyArray(Dest, Source, Count) Copy(Dest, Source, \
sizeof(*Source) * Count)

struct format_dest
{
    char* At;
    umm Size;
};

global_variable char GlobalDecChars[] = "0123456789";
global_variable char GlobalLowerHexChars[] = "0123456789abcdef";
global_variable char GlobalUpperHexChars[] = "0123456789ABCDEF";

internal void
ZeroSize(void* Ptr, memory_index Size)
{
    u8* Byte = (u8*)Ptr;
    
    while (Size--)
    {
        *Byte++ = 0;
    }
}

internal void*
Copy(void* DestInit, const void* SourceInit, memory_index Size)
{
    void* Result = 0;
    
    if (DestInit && SourceInit)
    {
        const u8* Source = (const u8*)SourceInit;
        u8* Dest = (u8*)DestInit;
        
        while (Size--)
        {
            *Dest++ = *Source++;
        }
        
        Result = DestInit;
    }
    
    return Result;
}

internal char*
CopyZ(void* Dest, const char* Z, umm Count)
{
    char* DestInit = (char*)Copy(Dest, Z, Count * sizeof(char));
    return DestInit;
}

internal char*
CopyZ(void* Dest, const char* Z)
{
    char* Result = CopyZ(Dest, Z, StringLength(Z));
    return Result;
}

internal string
MakeString(const char* Z, void* Memory, umm Count, memory_index MemorySize)
{
    // NOTE(yuval): Z is REQUIRED to be null terminated!
    string Str = { };
    
    if (Memory && Z)
    {
        Assert(Count * sizeof(char) <= MemorySize);
        
        Str.Data = CopyZ(Memory, Z, Count);
        Str.Count = Count;
        Str.MemorySize = MemorySize;
    }
    
    return Str;
}

internal string
MakeString(const char* Z, void* Memory, memory_index MemorySize)
{
    // NOTE(yuval): Z is REQUIRED to be null terminated!
    return MakeString(Z, Memory, StringLength(Z), MemorySize);
}

internal void
AppendZ(string* Dest, const char* Z, umm Count)
{
    if (Dest && Z)
    {
        if (((Dest->Count + Count) * sizeof(char)) <= Dest->MemorySize)
        {
            char* DestAt = Dest->Data + Dest->Count;
            CopyZ(DestAt, Z, Count);
            Dest->Count += Count;
        }
    }
}

internal void
AppendZ(string* Dest, const char* Z)
{
    AppendZ(Dest, Z, StringLength(Z));
}

internal void
AppendString(string* Dest, const string* Source)
{
    AppendZ(Dest, Source->Data, Source->Count);
}

internal void
AdvanceString(string* Value, umm Count)
{
    if (Value->Count >= Count)
    {
        Value->Data += Count;
        Value->Count -= Count;
        Value->MemorySize -= (Count * sizeof(char));
    }
    else
    {
        Value->Data += Value->Count;
        Value->Count = 0;
        Value->MemorySize = 0;
    }
}

internal string
WrapZ(char* Z)
{
    u32 ZLength = StringLength(Z);
    string Str = { Z, ZLength, ZLength * sizeof(char) };
    return Str;
}

internal void
CatStrings(char* Dest, memory_index DestCount,
           const char* SourceA, memory_index SourceACount,
           const char* SourceB, memory_index SourceBCount)
{
    Assert(DestCount > SourceACount + SourceBCount);
    char* DestAt = Dest;
    
    // TODO(yuval & eran): @Copy-and-paste
    // TODO(yuval & eran): @Incomplete use string struct
    for (u32 Index = 0; Index < SourceACount; ++Index)
    {
        *DestAt++ = SourceA[Index];
    }
    
    for (u32 Index = 0; Index < SourceBCount; ++Index)
    {
        *DestAt++ = SourceB[Index];
    }
    
    *DestAt = 0;
}

internal char
ToLowercase(char Value)
{
    char Result = Value;
    
    if ((Result >= 'A') && (Result <= 'Z'))
    {
        Result += 'a' - 'A';
    }
    
    return Result;
}

internal void
ToLowercase(char* Value)
{
    while (*Value)
    {
        *Value++ = ToLowercase(*Value);
    }
}

internal char
ToUppercase(char Value)
{
    char Result = Value;
    
    if ((Result >= 'a') && (Result <= 'z'))
    {
        Result -= 'a' - 'A';
    }
    
    return Result;
}

internal void
ToUppercase(char* Value)
{
    while (*Value)
    {
        *Value++ = ToUppercase(*Value);
    }
}

internal void
OutChar(format_dest* Dest, char Value)
{
    if (Dest->Size)
    {
        --Dest->Size;
        *Dest->At++ = Value;
    }
}

internal void
OutChars(format_dest* Dest, const char* Value)
{
    // TODO(yuval & eran): Speed this up
    while (*Value)
    {
        OutChar(Dest, *Value++);
    }
}

internal void
OutString(format_dest* Dest, string Value)
{
    while (Value.Count--)
    {
        OutChar(Dest, *Value.Data++);
    }
}

internal void
OutCharsLowercase(format_dest* Dest, const char* Value)
{
    while (*Value)
    {
        OutChar(Dest, ToLowercase(*Value++));
    }
}

internal void
OutCharsUppercase(format_dest* Dest, const char* Value)
{
    while (*Value)
    {
        OutChar(Dest, ToUppercase(*Value++));
    }
}

internal void
U64ToASCII(format_dest* Dest, u64 Value, u32 Base, const char* Digits)
{
    Assert(Base != 0);
    
    char* Start = Dest->At;
    
    do
    {
        u64 DigitIndex = Value % Base;
        char Digit = Digits[DigitIndex];
        OutChar(Dest, Digit);
        Value /= Base;
    }
    while (Value != 0);
    
    char* End = Dest->At;
    
    while (Start < End)
    {
        --End;
        
        // TODO(yuval, eran): Metaprogramming SWAP
        char Temp = *End;
        *End = *Start;
        *Start = Temp;
        ++Start;
    }
}

internal void
F64ToASCII(format_dest* Dest, f64 Value, u32 Precision)
{
    if (Value < 0)
    {
        OutChar(Dest, '-');
        Value = -Value;
    }
    
    u64 IntegerPart = (u64)Value;
    U64ToASCII(Dest, IntegerPart, 10, GlobalDecChars);
    
    Value -= IntegerPart;
    
    OutChar(Dest, '.');
    
    // TODO(yuval & eran): Round to the precision
    for (u32 PrecisionIndex = 0;
         PrecisionIndex < Precision;
         PrecisionIndex++)
    {
        Value *= 10.0;
        
        u32 Integer = (u32)Value;
        OutChar(Dest, GlobalDecChars[Integer]);
        
        Value -= Integer;
    }
}

internal s32
S32FromZInternal(const char** AtInit)
{
    s32 Result = 0;
    const char* At = *AtInit;
    
    while ((*At >= '0') && (*At <= '9'))
    {
        Result *= 10;
        Result += *At - '0';
        ++At;
    }
    
    *AtInit = At;
    return Result;
}

internal s32
S32FromZ(const char* At)
{
    const char* Ignored = At;
    s32 Result = S32FromZInternal(&Ignored);
    return Result;
}

internal umm
FormatStringList(char* DestInit, umm DestSize,
                 const char* Format, va_list ArgList)
{
    format_dest Dest = { DestInit, DestSize };
    
    if (Dest.Size)
    {
        const char* FormatAt = Format;
        
        while (FormatAt[0] && Dest.Size)
        {
            if (*FormatAt == '%')
            {
                ++FormatAt;
                
                // NOTE(yuval): Flag Handling
                bool Parsing = true;
                bool ForceSign = false;
                bool PadWithZeros = false;
                bool LeftJustify = false;
                bool PositiveSignIsBlank = false;
                bool AnnotateIfNotZero = false;
                
                while (Parsing)
                {
                    switch (*FormatAt)
                    {
                        case '+': { ForceSign = true; } break;
                        case '0': { PadWithZeros = true; } break;
                        case '-': { LeftJustify = true; } break;
                        case ' ': { PositiveSignIsBlank = true; } break;
                        case '#': { AnnotateIfNotZero = true; } break;
                        default: { Parsing = false; } break;
                    }
                    
                    if (Parsing)
                    {
                        ++FormatAt;
                    }
                }
                
                // NOTE(yuval): Width Handling
                b32 WidthSpecified = false;
                s32 Width = 0;
                
                if (*FormatAt == '*')
                {
                    Width = va_arg(ArgList, s32);
                    WidthSpecified = true;
                    ++FormatAt;
                }
                else if ((*FormatAt >= '0') && (*FormatAt <= '9'))
                {
                    Width = S32FromZInternal(&FormatAt);
                    WidthSpecified = true;
                }
                
                // NOTE(yuval): Precision Handling
                b32 PrecisionSpecified = false;
                u32 Precision = 0;
                
                if (*FormatAt == '.')
                {
                    ++FormatAt;
                    
                    if (*FormatAt == '*')
                    {
                        Precision = va_arg(ArgList, u32);
                        PrecisionSpecified = true;
                        ++FormatAt;
                    }
                    else if ((*FormatAt >= '0') && (*FormatAt <= '9'))
                    {
                        Precision = (u32)S32FromZInternal(&FormatAt);
                        PrecisionSpecified = true;
                    }
                    else
                    {
                        Assert(!"Malformed Precision Specifier!");
                    }
                }
                
                // TODO(yuval & eran): Maybe replace this
                if (!PrecisionSpecified)
                {
                    Precision = 6;
                }
                
                // NOTE(yuval): Length Handling
                u32 IntegerLength = 4;
                u32 FloatLength = 8;
                
                // TODO(yuval & eran): Change length based on specified flags
                // For now this flags are disabled
                
                // NOTE(yuval): Regular Flags Handling
                char TempBuffer[64];
                char* Temp = TempBuffer;
                format_dest TempDest = { Temp, ArrayCount(TempBuffer) };
                const char* Prefix = "";
                b32 IsFloat = false;
                
                switch (*FormatAt)
                {
                    case 'd':
                    case 'i':
                    {
                        s64 Value = ReadVarArgSignedInteger(IntegerLength, ArgList);
                        b32 WasNegative = Value < 0;
                        
                        if (WasNegative)
                        {
                            Value = -Value;
                        }
                        
                        U64ToASCII(&TempDest, (u64)Value, 10, GlobalDecChars);
                        
                        if (WasNegative)
                        {
                            Prefix = "-";
                        }
                        else if (ForceSign)
                        {
                            // TODO(yuval): Assert for PositiveSignIsBlank
                            Prefix = "+";
                        }
                        else if (PositiveSignIsBlank)
                        {
                            Prefix = " ";
                        }
                    } break;
                    
                    case 'u':
                    {
                        u64 Value = ReadVarArgUnsignedInteger(IntegerLength, ArgList);
                        U64ToASCII(&TempDest, Value, 10, GlobalDecChars);
                    } break;
                    
                    case 'o':
                    {
                        u64 Value = ReadVarArgUnsignedInteger(IntegerLength, ArgList);
                        U64ToASCII(&TempDest, Value, 8, GlobalDecChars);
                        
                        if (AnnotateIfNotZero && (Value > 0))
                        {
                            Prefix = "0";
                        }
                    } break;
                    
                    case 'x':
                    {
                        u64 Value = ReadVarArgUnsignedInteger(IntegerLength, ArgList);
                        U64ToASCII(&TempDest, Value, 16, GlobalLowerHexChars);
                        
                        if (AnnotateIfNotZero && (Value > 0))
                        {
                            Prefix = "0x";
                        }
                    } break;
                    
                    case 'X':
                    {
                        u64 Value = ReadVarArgUnsignedInteger(IntegerLength, ArgList);
                        U64ToASCII(&TempDest, Value, 16, GlobalUpperHexChars);
                        
                        if (AnnotateIfNotZero && (Value > 0))
                        {
                            Prefix = "0X";
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
                        f64 Value = ReadVarArgFloat(FloatLength, ArgList);
                        F64ToASCII(&TempDest, Value, Precision);
                        IsFloat = true;
                    } break;
                    
                    case 'c':
                    {
                        s32 Value = va_arg(ArgList, s32);
                        OutChar(&TempDest, (char)Value);
                    } break;
                    
                    case 's':
                    {
                        char* Value = va_arg(ArgList, char*);
                        Temp = Value;
                        
                        if (PrecisionSpecified)
                        {
                            TempDest.Size = 0;
                            
                            for (char* Scan = Value;
                                 *Scan && (TempDest.Size < Precision);
                                 ++Scan)
                            {
                                ++TempDest.Size;
                            }
                        }
                        else
                        {
                            TempDest.Size = StringLength(Value);
                        }
                        
                        TempDest.At = Value + TempDest.Size;
                    } break;
                    
                    /*
                    case 'S':
                    {
                    
                    } break;*/
                    
                    case 'p':
                    {
                        void* Value = va_arg(ArgList, void*);
                        U64ToASCII(&TempDest, *(umm*)&Value, 16, GlobalLowerHexChars);
                    } break;
                    
                    case 'n':
                    {
                        s32* TabDest = va_arg(ArgList, s32*);
                        *TabDest = (int)(Dest.At - DestInit);
                    } break;
                    
                    case '%':
                    {
                        OutChar(&TempDest, '%');
                    } break;
                    
                    default:
                    {
                        Assert(!"Unrecognized Format Specifier!");
                    } break;
                }
                
                if (TempDest.At - Temp)
                {
                    smm UsePrecision = Precision;
                    if (IsFloat || !PrecisionSpecified)
                    {
                        UsePrecision = TempDest.At - Temp;
                    }
                    
                    smm PrefixLength = StringLength(Prefix);
                    smm UseWidth = Width;
                    smm ComputedWidth = UsePrecision + PrefixLength;
                    
                    if (UseWidth < ComputedWidth)
                    {
                        UseWidth = ComputedWidth;
                    }
                    
                    if (PadWithZeros)
                    {
                        // TODO(yuval & eran): Assert for left justify
                        LeftJustify = false;
                    }
                    
                    if (!LeftJustify)
                    {
                        while (UseWidth > ComputedWidth)
                        {
                            OutChar(&Dest, PadWithZeros ? '0' : ' ');
                            --UseWidth;
                        }
                    }
                    
                    for (const char* Pre = Prefix; *Pre && UseWidth; ++Pre)
                    {
                        OutChar(&Dest, *Pre);
                        --UseWidth;
                    }
                    
                    if (UsePrecision > UseWidth)
                    {
                        UsePrecision = UseWidth;
                    }
                    
                    while (UsePrecision > (TempDest.At - Temp))
                    {
                        OutChar(&Dest, '0');
                        --UsePrecision;
                        --UseWidth;
                    }
                    
                    while (UsePrecision && (TempDest.At != Temp))
                    {
                        OutChar(&Dest, *Temp++);
                        --UsePrecision;
                        --UseWidth;
                    }
                    
                    if (LeftJustify)
                    {
                        while (UseWidth)
                        {
                            OutChar(&Dest, ' ');
                            --UseWidth;
                        }
                    }
                }
                
                if (*FormatAt)
                {
                    ++FormatAt;
                }
            }
            else
            {
                OutChar(&Dest, *FormatAt++);
            }
        }
        
        if (Dest.Size)
        {
            Dest.At[0] = 0;
        }
        else
        {
            Dest.At[-1] = 0;
        }
    }
    
    umm Result = Dest.At - DestInit;
    return Result;
}

internal umm
FormatString(char* Dest, umm DestSize, const char* Format, ...)
{
    va_list ArgList;
    
    va_start(ArgList, Format);
    umm Result = FormatStringList(Dest, DestSize, Format, ArgList);
    va_end(ArgList);
    
    return Result;
}

#define GAME_SHARED_H
#endif

