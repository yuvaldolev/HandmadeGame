#include <stdlib.h> // TODO(yuval & eran): Remove this

#define LOG_FORMAT_FN(name) void name(LogMsg* msg, struct LogFormatter* formatter)
typedef LOG_FORMAT_FN(LogFormatFnType);

struct LogFormatter
{
    LogFormatFnType* fn;
    String chars;

    LogFormatter* next;
};

global_variable GameMemory* globalLogFormatMemory;
global_variable MemoryArena* globalArena = 0;
global_variable LogFormatter* globalFirstFormatter;

internal void
LogFormatWriteString(LogMsg* msg, String value)
{
    FormatDest dest = { msg->formattedAt, msg->remainingFormattingSpace };
    OutString(&dest, value);

    // TODO(yuval & eran): Refactor this into a function
    msg->formattedAt = dest.at;
    msg->remainingFormattingSpace = dest.size;

    if (msg->remainingFormattingSpace)
    {
        *msg->formattedAt = '\0';
    }
}

internal void
LogFormatWriteChars(LogMsg* msg, const char* value)
{
    FormatDest dest = { msg->formattedAt, msg->remainingFormattingSpace };
    OutChars(&dest, value);

    msg->formattedAt = dest.at;
    msg->remainingFormattingSpace = dest.size;

    if (msg->remainingFormattingSpace)
    {
        *msg->formattedAt = '\0';
    }
}

internal
LOG_FORMAT_FN(LogFormatUserMessage)
{
    umm bytesWritten = FormatStringList(msg->formattedAt,
                                        msg->remainingFormattingSpace,
                                        msg->format, *msg->argList);

    msg->remainingFormattingSpace -= bytesWritten;
    msg->formattedAt += bytesWritten;
}

internal
LOG_FORMAT_FN(LogFormatNewLine)
{
    *msg->formattedAt++ = '\n';
    --msg->remainingFormattingSpace;

    if (msg->remainingFormattingSpace)
    {
        *msg->formattedAt = '\0';
    }
}

internal
LOG_FORMAT_FN(LogFormatDate)
{
    PlatformDateTime dateTime = globalLogFormatMemory->PlatformGetDateTime();
    umm bytesWritten = FormatString(msg->formattedAt, msg->remainingFormattingSpace,
                                    "%02u/%02u/%04u %02u:%02u:%02u:%03u",
                                    dateTime.day, dateTime.month, dateTime.year,
                                    dateTime.hour, dateTime.minute, dateTime.second,
                                    dateTime.milliseconds);

    msg->remainingFormattingSpace -= bytesWritten;
    msg->formattedAt += bytesWritten;
}

internal
LOG_FORMAT_FN(LogFormatFullFileName)
{
    const char* fileName = msg->file;

    if (!fileName)
    {
        fileName = "(file=null)";
    }

    LogFormatWriteChars(msg, fileName);
}

internal
LOG_FORMAT_FN(LogFormatFileName)
{
    const char* fileName = msg->file;

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

    LogFormatWriteChars(msg, fileName);
}

internal
LOG_FORMAT_FN(LogFormatFnName)
{
    const char* fnName = msg->fn;

    if (!fnName)
    {
        fnName = "(fn=null)";
    }

    LogFormatWriteChars(msg, fnName);
}

internal
LOG_FORMAT_FN(LogFormatLineNum)
{
    FormatDest dest = { msg->formattedAt, msg->remainingFormattingSpace };
    U64ToASCII(&dest, msg->line, 10, globalDecChars);

    msg->formattedAt = dest.at;
    msg->remainingFormattingSpace = dest.size;

    if (msg->remainingFormattingSpace)
    {
        *msg->formattedAt = '\0';
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
        case LogLevelFatal: { return "Fatal";} break;
        default: { return "(level=null)"; } break;
    }
}

internal
LOG_FORMAT_FN(LogFormatLevelUppercase)
{
    const char* levelString = LogFormatGetLevelString(msg->level);
    FormatDest dest = { msg->formattedAt, msg->remainingFormattingSpace };
    OutCharsUppercase(&dest, levelString);

    msg->formattedAt = dest.at;
    msg->remainingFormattingSpace = dest.size;

    if (msg->remainingFormattingSpace)
    {
        *msg->formattedAt = '\0';
    }
}

internal
LOG_FORMAT_FN(LogFormatLevelTitlecase)
{
    const char* levelString = LogFormatGetLevelString(msg->level);
    LogFormatWriteChars(msg, levelString);
}

internal
LOG_FORMAT_FN(LogFormatPercent)
{
    *msg->formattedAt++ = '%';
    --msg->remainingFormattingSpace;

    if (msg->remainingFormattingSpace)
    {
        *msg->formattedAt = '\0';
    }
}

internal
LOG_FORMAT_FN(LogFormatChars)
{
    LogFormatWriteString(msg, formatter->chars);
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
            char* memory = (char*)PushSize(globalArena, sizeof(char));
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
                formatter = PushStruct(globalArena, LogFormatter);
                // ZeroSize(format, sizeof(LogFormat)); TODO(yuval & eran): Use this
                formatter->fn = formatFn;
                formatter->chars = formatterChars;
            }
        }
    }

    return formatter;
}

void
LogFormatSetPattern(MemoryArena* arena, const char* fmt)
{
    if (fmt)
    {
        // TODO(yuval & eran): Call LogFormatClean()
        globalArena = arena;
        globalFirstFormatter = LogFormatGetNextFormatter(&fmt);

        LogFormatter* currFormatter = globalFirstFormatter;

        while (currFormatter)
        {
            currFormatter = LogFormatAppendFormatter(currFormatter,
                                                     LogFormatGetNextFormatter(&fmt));
        }
    }
}

void
LogFormatClean()
{
    // TODO(yuval & eran): Implement this
}

void
LogFormatMessage(LogMsg* msg)
{
    LogFormatter* formatterAt = globalFirstFormatter;

    while (formatterAt && msg->remainingFormattingSpace)
    {
        formatterAt->fn(msg, formatterAt);
        formatterAt = formatterAt->next;
    }
}

