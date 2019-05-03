#include <stdlib.h> // TODO(yuval & eran): Remove this

#define LOG_FORMAT_FN(name) void name(LogMsg* msg, struct LogFormat* format)
typedef LOG_FORMAT_FN(LogFormatFnType);

struct LogFormat
{
    LogFormatFnType* fn;
    String chars;
};

struct LogFormats
{
    LogFormat** formats;
    umm size;
};

global_variable MemoryArena* globalArena = 0;
global_variable LogFormats* globalFormats;

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
                                        msg->format, msg->argList);
    
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
    PlatformDateTime dateTime = PlatformGetDateTime();
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
    LogFormatWriteChars(msg, format->chars);
}

internal void*
LogFormatReallocateMemory(void* memory, umm newBlockCount,
                          umm prevBlockCount, umm blockSize)
{
    // TODO(yuval & eran): Temporary! Replace dynamic memory allocation
    // with memory arena
    memory_index newMemorySize = blockSize * newBlockCount;
    s8* newMemory = (s8*)malloc(newMemorySize);
    ZeroSize(newMemory, newMemorySize);
    
    if (memory)
    {
        Copy(newMemory, memory, blockSize * prevBlockCount);
        free(memory);
    }
    
    return newMemory;
}

internal void
LogFormatAppendFormat(LogFormats* formats, LogFormat* format,
                      int* index)
{
    const int CHUNK_SIZE = 5;
    
    umm* size = &formats->size;
    
    if (*index >= *size)
    {
        formats->formats = (LogFormat**)
            LogFormatReallocateMemory(formats->formats,
                                      *size + CHUNK_SIZE,
                                      *size,
                                      sizeof(LogFormat*));
        *size += CHUNK_SIZE;
    }
    
    formats->formats[*index] = format;
    ++(*index);
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

internal LogFormat*
LogFormatGetNextFormat(const char** fmt)
{
    LogFormat* format = 0;
    
    if (fmt && *fmt)
    {
        char formatSpecifier = **fmt;
        
        if (formatSpecifier)
        {
            LogFormatFnType* formatFn = 0;
            String formatChars = { };
            
            if (formatSpecifier == '%')
            {
                formatSpecifier = LogFormatAdvanceChars(fmt, 1);
                formatFn = LogFormatGetFormatFn(formatSpecifier);
                LogFormatAdvanceChars(fmt, 1);
            }
            else
            {
                formatFn = LogFormatChars;
                formatChars = LogFormatGetNextChars(fmt);
            }
            
            if (formatFn)
            {
                format = PushStruct(globalArena, LogFormat);
                // ZeroSize(format, sizeof(LogFormat)); TODO(yuval & eran): Use this
                format->fn = formatFn;
                format->chars = formatChars;
            }
        }
    }
    
    return format;
}

void
LogFormatSetPattern(MemoryArena* arena, const char* fmt)
{
    if (fmt)
    {
        //LogFormatClean();
        globalArena = arena;
        globalFormats = PushStruct(arena, LogFormats);
        
        int currFormatIndex = 0;
        LogFormat* format = LogFormatGetNextFormat(&fmt);
        
        while (format)
        {
            LogFormatAppendFormat(&globalFormats, format, &currFormatIndex);
            format = LogFormatGetNextFormat(&fmt);
        }
    }
}

void
LogFormatClean()
{
    LogFormat** formatsAt = globalFormats.formats;
    
    if (formatsAt)
    {
        umm size = globalFormats.size;
        
        while (*formatsAt && size--)
        {
            LogFormat* format = *formatsAt;
            
            // NOTE(yuval): Delete the format's chars
            free(format->chars); // TODO(yuval & eran): Temporary
            format->chars = 0;
            
            // NOTE(yuval): Delete the format
            free(format);
            *formatsAt = 0;
            
            ++formatsAt;
        }
        
        // NOTE(yuval): Delete the formats array
        free(globalFormats.formats);
        globalFormats.formats = 0;
        
        // NOTE(yuval): Zero the size
        globalFormats.size = 0;
    }
}

void
LogFormatMessage(LogMsg* msg)
{
    LogFormat** formatsAt = globalFormats.formats;
    umm size = globalFormats.size;
    
    while (*formatsAt && size-- && msg->remainingFormattingSpace)
    {
        (*formatsAt)->fn(msg, *formatsAt);
        ++formatsAt;
    }
}

