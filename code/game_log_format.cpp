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
global_variable LogFormatter* GlobalFirstFormatter;

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
    
    Msg->FormattedAt = Dest.at;
    Msg->RemainingFormattingSpace = Dest.size;
    
    if (Msg->RemainingFormattingSpace)
    {
        *Msg->FormattedAt = '\0';
    }
}

internal
LOG_FORMAT_FN(LogFormatUserMessage)
{
    umm bytesWritten = FormatStringList(Msg->FormattedAt,
                                        Msg->RemainingFormattingSpace,
                                        Msg->Format, *Msg->ArgList);
    
    Msg->RemainingFormattingSpace -= bytesWritten;
    Msg->FormattedAt += bytesWritten;
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
    umm bytesWritten = FormatString(Msg->FormattedAt, Msg->RemainingFormattingSpace,
                                    "%02u/%02u/%04u %02u:%02u:%02u:%03u",
                                    dateTime.day, dateTime.month, dateTime.year,
                                    dateTime.hour, dateTime.minute, dateTime.second,
                                    dateTime.milliseconds);
    
    Msg->RemainingFormattingSpace -= bytesWritten;
    Msg->FormattedAt += bytesWritten;
}

internal
LOG_FORMAT_FN(LogFormatFullFileName)
{
    const char* fileName = Msg->file;
    
    if (!fileName)
    {
        fileName = "(file=null)";
    }
    
    LogFormatWriteChars(Msg, fileName);
}

internal
LOG_FORMAT_FN(LogFormatFileName)
{
    const char* fileName = Msg->file;
    
    if (fileName)
    {
        const char* fileNameAt = fileName;
        
        while (*fileNameAt)
        {
            if ((*fileNameAt == '/') || (*fileNameAt == '\\'))
            {
                fileName = fileNameAt + 1;
            }
            
            ++fileNameAt;
        }
    }
    else
    {
        fileName = "(file=null)";
    }
    
    LogFormatWriteChars(Msg, fileName);
}

internal
LOG_FORMAT_FN(LogFormatFnName)
{
    const char* fnName = Msg->fn;
    
    if (!fnName)
    {
        fnName = "(fn=null)";
    }
    
    LogFormatWriteChars(Msg, fnName);
}

internal
LOG_FORMAT_FN(LogFormatLineNum)
{
    format_dest Dest = { Msg->FormattedAt, Msg->RemainingFormattingSpace };
    U64ToASCII(&Dest, Msg->line, 10, GlobalDecChars);
    
    Msg->FormattedAt = Dest.at;
    Msg->RemainingFormattingSpace = Dest.size;
    
    if (Msg->RemainingFormattingSpace)
    {
        *Msg->FormattedAt = '\0';
    }
}

internal const char*
LogFormatGetLevelString(LogLevelEnum level)
{
    switch (level)
    {
        case LogLevelDebug: { return "Debug"; } break;
        case LogLevelInfo: { return "Info"; } break;
        case LogLevelWarn: { return "Warn"; } break;
        case LogLevelError: { return "Error"; } break;
        case LogLevelFatal: { return "Fatal"; } break;
    }
}

internal
LOG_FORMAT_FN(LogFormatLevelUppercase)
{
    const char* levelString = LogFormatGetLevelString(Msg->level);
    format_dest Dest = { Msg->FormattedAt, Msg->RemainingFormattingSpace };
    OutCharsUppercase(&Dest, levelString);
    
    Msg->FormattedAt = Dest.at;
    Msg->RemainingFormattingSpace = Dest.size;
    
    if (Msg->RemainingFormattingSpace)
    {
        *Msg->FormattedAt = '\0';
    }
}

internal
LOG_FORMAT_FN(LogFormatLevelTitlecase)
{
    const char* levelString = LogFormatGetLevelString(Msg->level);
    LogFormatWriteChars(Msg, levelString);
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
    LogFormatWriteString(Msg, formatter->chars);
}

internal LogFormatter*
LogFormatAppendFormatter(LogFormatter* formatter, LogFormatter* next)
{
    LogFormatter* result = 0;
    
    if (formatter)
    {
        formatter->next = next;
        result = next;
    }
    
    return result;
}


internal char
LogFormatAdvanceChars(const char** fmt, u32 amount)
{
    *fmt += amount;
    return **fmt;
}

internal String
LogFormatGetNextChars(const char** fmt)
{
    String result =  { };
    
    if (fmt && *fmt)
    {
        char currChar = **fmt;
        b32 dataInitialized = false;
        
        while (currChar && currChar != '%')
        {
            char* memory = (char*)PushSize(GlobalLogFormatArena, sizeof(char));
            *memory = currChar;
            
            if (!dataInitialized)
            {
                result.data = memory;
                dataInitialized = true;
            }
            
            ++result.count;
            result.memorySize += sizeof(char);
            
            currChar = LogFormatAdvanceChars(fmt, 1);
        }
    }
    
    return result;
}

internal LogFormatFnType*
LogFormatGetFormatFn(const char tokenSpecifier)
{
    LogFormatFnType* formatFn;
    
    switch (tokenSpecifier)
    {
        case 'm':
        {
            formatFn = LogFormatUserMessage;
        } break;
        
        case 'n':
        {
            formatFn = LogFormatNewLine;
        } break;
        
        case 'd':
        {
            formatFn = LogFormatDate;
        } break;
        
        case 'F':
        {
            formatFn = LogFormatFullFileName;
        } break;
        
        case 'f':
        {
            formatFn = LogFormatFileName;
        } break;
        
        case 'U':
        {
            formatFn = LogFormatFnName;
        } break;
        
        case 'L':
        {
            formatFn = LogFormatLineNum;
        } break;
        
        case 'V':
        {
            formatFn = LogFormatLevelUppercase;
        } break;
        
        case 'v':
        {
            formatFn = LogFormatLevelTitlecase;
        } break;
        
        case '%':
        {
            formatFn = LogFormatPercent;
        } break;
        
        default:
        {
            // TODO(yuval & eran): Assert
            formatFn = 0;
        }
    }
    
    return formatFn;
}

internal LogFormatter*
LogFormatGetNextFormatter(const char** fmt)
{
    LogFormatter* formatter = 0;
    
    if (fmt && *fmt)
    {
        char formatSpecifier = **fmt;
        
        if (formatSpecifier)
        {
            LogFormatFnType* formatFn = 0;
            String formatterChars = { };
            
            if (formatSpecifier == '%')
            {
                formatSpecifier = LogFormatAdvanceChars(fmt, 1);
                formatFn = LogFormatGetFormatFn(formatSpecifier);
                LogFormatAdvanceChars(fmt, 1);
            }
            else
            {
                formatFn = LogFormatChars;
                formatterChars = LogFormatGetNextChars(fmt);
            }
            
            if (formatFn)
            {
                formatter = PushStruct(GlobalLogFormatArena, LogFormatter);
                // ZeroSize(format, sizeof(LogFormat)); TODO(yuval & eran): Use this
                formatter->fn = formatFn;
                formatter->chars = formatterChars;
            }
        }
    }
    
    return formatter;
}

internal void
LogFormatSetPattern(MemoryArena* arena, const char* fmt)
{
    if (fmt)
    {
        // TODO(yuval & eran): Call LogFormatClean()
        GlobalLogFormatArena = arena;
        GlobalFirstFormatter = LogFormatGetNextFormatter(&fmt);
        
        LogFormatter* currFormatter = GlobalFirstFormatter;
        
        while (currFormatter)
        {
            currFormatter = LogFormatAppendFormatter(currFormatter,
                                                     LogFormatGetNextFormatter(&fmt));
        }
    }
}

internal void
LogFormatClean()
{
    // TODO(yuval & eran): Implement this
}

internal void
LogFormatMessage(log_msg* Msg)
{
    LogFormatter* FormatterAt = GlobalFirstFormatter;
    
    while (FormatterAt && Msg->RemainingFormattingSpace)
    {
        FormatterAt->fn(Msg, FormatterAt);
        FormatterAt = FormatterAt->next;
    }
}

