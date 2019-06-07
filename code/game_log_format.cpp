#include <stdlib.h> // TODO(yuval & eran): Remove this

#define LOG_FORMAT_FN(name) void name(log_msg* Msg, struct log_formatter* Formatter)
typedef LOG_FORMAT_FN(log_format_fn);

struct log_formatter
{
    log_format_fn* Fn;
    string Chars;
    
    log_formatter* Next;
};

global_variable memory_arena* GlobalLogFormatArena = 0;
global_variable log_formatter* GlobalFirstFormatter;

internal void
LogFormatWriteString(log_msg* Msg, string Value)
{
    format_dest Dest = { Msg->FormattedAt, Msg->RemainingFormattingSpace };
    OutString(&Dest, Value);
    
    // TODO(yuval & eran): Refactor this into a function
    Msg->FormattedAt = Dest.At;
    Msg->RemainingFormattingSpace = Dest.Size;
    
    if (Msg->RemainingFormattingSpace)
    {
        *Msg->FormattedAt = '\0';
    }
}

internal void
LogFormatWriteChars(log_msg* Msg, const char* Value)
{
    format_dest Dest = { Msg->FormattedAt, Msg->RemainingFormattingSpace };
    OutChars(&Dest, Value);
    
    Msg->FormattedAt = Dest.At;
    Msg->RemainingFormattingSpace = Dest.Size;
    
    if (Msg->RemainingFormattingSpace)
    {
        *Msg->FormattedAt = '\0';
    }
}

internal
LOG_FORMAT_FN(LogFormatUserMessage)
{
    umm BytesWritten = FormatStringList(Msg->FormattedAt,
                                        Msg->RemainingFormattingSpace,
                                        Msg->Format, *Msg->ArgList);
    
    Msg->RemainingFormattingSpace -= BytesWritten;
    Msg->FormattedAt += BytesWritten;
}

internal
LOG_FORMAT_FN(LogFormatNewLine)
{
    *Msg->FormattedAt++ = '\n';
    --Msg->RemainingFormattingSpace;
    
    if (Msg->RemainingFormattingSpace)
    {
        *Msg->FormattedAt = '\0';
    }
}

internal
LOG_FORMAT_FN(LogFormatDate)
{
    platform_date_time DateTime = Platform.GetDateTime();
    umm BytesWritten = FormatString(Msg->FormattedAt, Msg->RemainingFormattingSpace,
                                    "%02u/%02u/%04u %02u:%02u:%02u:%03u",
                                    DateTime.Day, DateTime.Month, DateTime.Year,
                                    DateTime.Hour, DateTime.Minute, DateTime.Second,
                                    DateTime.Milliseconds);
    
    Msg->RemainingFormattingSpace -= BytesWritten;
    Msg->FormattedAt += BytesWritten;
}

internal
LOG_FORMAT_FN(LogFormatFullFileName)
{
    const char* FileName = Msg->File;
    
    if (!FileName)
    {
        FileName = "(file=null)";
    }
    
    LogFormatWriteChars(Msg, FileName);
}

internal
LOG_FORMAT_FN(LogFormatFileName)
{
    const char* FileName = Msg->File;
    
    if (FileName)
    {
        const char* FileNameAt = FileName;
        
        while (*FileNameAt)
        {
            if ((*FileNameAt == '/') || (*FileNameAt == '\\'))
            {
                FileName = FileNameAt + 1;
            }
            
            ++FileNameAt;
        }
    }
    else
    {
        FileName = "(file=null)";
    }
    
    LogFormatWriteChars(Msg, FileName);
}

internal
LOG_FORMAT_FN(LogFormatFnName)
{
    const char* FnName = Msg->Fn;
    
    if (!FnName)
    {
        FnName = "(fn=null)";
    }
    
    LogFormatWriteChars(Msg, FnName);
}

internal
LOG_FORMAT_FN(LogFormatLineNum)
{
    format_dest Dest = { Msg->FormattedAt, Msg->RemainingFormattingSpace };
    U64ToASCII(&Dest, Msg->Line, 10, GlobalDecChars);
    
    Msg->FormattedAt = Dest.At;
    Msg->RemainingFormattingSpace = Dest.Size;
    
    if (Msg->RemainingFormattingSpace)
    {
        *Msg->FormattedAt = '\0';
    }
}

internal const char*
LogFormatGetLevelString(log_level Level)
{
    switch (Level)
    {
        case LogLevel_Debug: { return "Debug"; } break;
        case LogLevel_Info: { return "Info"; } break;
        case LogLevel_Warn: { return "Warn"; } break;
        case LogLevel_Error: { return "Error"; } break;
        case LogLevel_Fatal: { return "Fatal"; } break;
    }
    
    return "(level=null)";
}

internal
LOG_FORMAT_FN(LogFormatLevelUppercase)
{
    const char* LevelString = LogFormatGetLevelString(Msg->Level);
    format_dest Dest = { Msg->FormattedAt, Msg->RemainingFormattingSpace };
    OutCharsUppercase(&Dest, LevelString);
    
    Msg->FormattedAt = Dest.At;
    Msg->RemainingFormattingSpace = Dest.Size;
    
    if (Msg->RemainingFormattingSpace)
    {
        *Msg->FormattedAt = '\0';
    }
}

internal
LOG_FORMAT_FN(LogFormatLevelTitlecase)
{
    const char* LevelString = LogFormatGetLevelString(Msg->Level);
    LogFormatWriteChars(Msg, LevelString);
}

internal
LOG_FORMAT_FN(LogFormatPercent)
{
    *Msg->FormattedAt++ = '%';
    --Msg->RemainingFormattingSpace;
    
    if (Msg->RemainingFormattingSpace)
    {
        *Msg->FormattedAt = '\0';
    }
}

internal
LOG_FORMAT_FN(LogFormatChars)
{
    LogFormatWriteString(Msg, Formatter->Chars);
}

internal log_formatter*
LogFormatAppendFormatter(log_formatter* Formatter, log_formatter* Next)
{
    log_formatter* Result = 0;
    
    if (Formatter)
    {
        Formatter->Next = Next;
        Result = Next;
    }
    
    return Result;
}


internal char
LogFormatAdvanceChars(const char** Fmt, u32 amount)
{
    *Fmt += amount;
    return **Fmt;
}

internal string
LogFormatGetNextChars(const char** Fmt)
{
    string Result =  { };
    
    if (Fmt && *Fmt)
    {
        char CurrChar = **Fmt;
        b32 DataInitialized = false;
        
        while (CurrChar && CurrChar != '%')
        {
            char* Memory = (char*)PushSize(GlobalLogFormatArena, sizeof(char));
            *Memory = CurrChar;
            
            if (!DataInitialized)
            {
                Result.Data = Memory;
                DataInitialized = true;
            }
            
            ++Result.Count;
            Result.MemorySize += sizeof(char);
            
            CurrChar = LogFormatAdvanceChars(Fmt, 1);
        }
    }
    
    return Result;
}

internal log_format_fn*
LogFormatGetFormatFn(const char TokenSpecifier)
{
    log_format_fn* FormatFn;
    
    switch (TokenSpecifier)
    {
        case 'm':
        {
            FormatFn = LogFormatUserMessage;
        } break;
        
        case 'n':
        {
            FormatFn = LogFormatNewLine;
        } break;
        
        case 'd':
        {
            FormatFn = LogFormatDate;
        } break;
        
        case 'F':
        {
            FormatFn = LogFormatFullFileName;
        } break;
        
        case 'f':
        {
            FormatFn = LogFormatFileName;
        } break;
        
        case 'U':
        {
            FormatFn = LogFormatFnName;
        } break;
        
        case 'L':
        {
            FormatFn = LogFormatLineNum;
        } break;
        
        case 'V':
        {
            FormatFn = LogFormatLevelUppercase;
        } break;
        
        case 'v':
        {
            FormatFn = LogFormatLevelTitlecase;
        } break;
        
        case '%':
        {
            FormatFn = LogFormatPercent;
        } break;
        
        default:
        {
            // TODO(yuval & eran): Assert
            FormatFn = 0;
        }
    }
    
    return FormatFn;
}

internal log_formatter*
LogFormatGetNextFormatter(const char** Fmt)
{
    log_formatter* Formatter = 0;
    
    if (Fmt && *Fmt)
    {
        char FormatSpecifier = **Fmt;
        
        if (FormatSpecifier)
        {
            log_format_fn* FormatFn = 0;
            string FormatterChars = { };
            
            if (FormatSpecifier == '%')
            {
                FormatSpecifier = LogFormatAdvanceChars(Fmt, 1);
                FormatFn = LogFormatGetFormatFn(FormatSpecifier);
                LogFormatAdvanceChars(Fmt, 1);
            }
            else
            {
                FormatFn = LogFormatChars;
                FormatterChars = LogFormatGetNextChars(Fmt);
            }
            
            if (FormatFn)
            {
                Formatter = PushStruct(GlobalLogFormatArena, log_formatter);
                // ZeroSize(format, sizeof(LogFormat)); TODO(yuval & eran): Use this
                Formatter->Fn = FormatFn;
                Formatter->Chars = FormatterChars;
            }
        }
    }
    
    return Formatter;
}

internal void
LogFormatSetPattern(memory_arena* Arena, const char* Fmt)
{
    if (Fmt)
    {
        // TODO(yuval, eran): Call LogFormatClean()
        GlobalLogFormatArena = Arena;
        GlobalFirstFormatter = LogFormatGetNextFormatter(&Fmt);
        
        log_formatter* CurrFormatter = GlobalFirstFormatter;
        
        while (CurrFormatter)
        {
            CurrFormatter = LogFormatAppendFormatter(CurrFormatter,
                                                     LogFormatGetNextFormatter(&Fmt));
        }
    }
}

internal void
LogFormatClean()
{
    // TODO(yuval, eran): Implement this
}

internal void
LogFormatMessage(log_msg* Msg)
{
    log_formatter* FormatterAt = GlobalFirstFormatter;
    
    while (FormatterAt && Msg->RemainingFormattingSpace)
    {
        FormatterAt->Fn(Msg, FormatterAt);
        FormatterAt = FormatterAt->Next;
    }
}

