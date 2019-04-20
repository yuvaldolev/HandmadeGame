#define LOG_FORMAT_FN(name) void name(LogMsg* msg)
typedef LOG_FORMAT_FN(LogFormatFnType);

struct LogFormats
{
    LogFormatFnType** funcs;
    umm funcsSize;

    char** chars;
    umm charsSize;
    memory_index charsIndex;
};

global_variable LogFormats globalFormats = { };

internal
LOG_FORMAT_FN(LogFormatUserMessage)
{
    umm bytesWritten = FormatStringList(msg->formattedAt,
                                        msg->remainingFormattingSpace,
                                        msg->format, msg->argList);
    msg->remainingFormattingSpace -= bytesWritten;
    msg->formattedAt += bytesWritten;
}

/*
LOG_FORMAT_FN(LogFormatNewLine);
LOG_FORMAT_FN(LogFormatDate);
LOG_FORMAT_FN(LogFormatFullFileName);
LOG_FORMAT_FN(LogFormatFileName);
LOG_FORMAT_FN(LogFormatFuncName);
LOG_FORMAT_FN(LogFormatLineNum);
LOG_FORMAT_FN(LogFormatLevelUppercase);
LOG_FORMAT_FN(LogFormatLevelTitlecase);
LOG_FORMAT_FN(LogFormatPercent);
LOG_FORMAT_FN(LogFormatChars);
*/

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
        Copy(memory, newMemory, blockSize * prevBlockCount);
        free(memory);
    }

    return newMemory;
}

internal void
LogFormatAppendChars(LogFormats* formats, char* chars)
{
    const int CHUNK_SIZE = 5;

    umm* size = &formats->charsSize;
    umm* index = &formats->charsIndex;

    if (*index >= *size)
    {
        formats->chars = (char**)LogFormatReallocateMemory(formats->chars,
                                                           *size + CHUNK_SIZE,
                                                           *size,
                                                           sizeof(char*));
        *size += CHUNK_SIZE;
    }

    formats->chars[*index] = chars;
    *(index)++;
}

internal void
LogFormatAppendToken(LogFormats* formats, LogFormatFnType* token,
                     int* index)
{
    const int CHUNK_SIZE = 5;

    umm* size = &formats->funcsSize;

    if (*index >= *size)
    {
        formats->funcs = (LogFormatFnType**)
            LogFormatReallocateMemory(formats->funcs,
                                      *size + CHUNK_SIZE,
                                      *size,
                                      sizeof(LogFormatFnType*));
        *size += CHUNK_SIZE;
    }

    formats->funcs[*index] = token;
    *(index)++;
}


internal char
LogFormatAdvanceChars(const char** fmt, u32 amount)
{
    *fmt += amount;
    return **fmt;
}

internal char*
LogFormatGetNextChars(const char** fmt)
{
    const u32 CHUNK_SIZE = 10;
    char* result = 0;
    umm resultLen = 0;
    memory_index resultIndex = 0;

    if (fmt && *fmt)
    {
        char currChar = **fmt;

        while (currChar)
        {
            result[resultIndex++] = currChar;
            currChar = LogFormatAdvanceChars(fmt, 1);

            if (resultIndex >= resultLen - 1)
            {
                result[resultIndex++] = '\0';

                result = (char*)
                    LogFormatReallocateMemory(result,
                                              resultLen + CHUNK_SIZE,
                                              resultLen,
                                              sizeof(char));
                resultLen += CHUNK_SIZE;
            }
        }
    }

    return result;
}

internal LogFormatFnType*
LogFormatGetToken(const char tokenSpecifier)
{
    LogFormatFnType* format;

    switch (tokenSpecifier)
    {
        case 'm':
        {
            format = LogFormatUserMessage;
        } break;
/*
        case 'n':
        {
            format = LogFormatNewLine;
        } break;

        case 'd':
        {
            format = LogFormatDate;
        } break;

        case 'F':
        {
            format = LogFormatFullFileName;
        } break;

        case 'f':
        {
            format = LogFormatFileName;
        } break;

        case 'U':
        {
            format = LogFormatFuncName;
        } break;

        case 'L':
        {
            format = LogFormatLineNum;
        } break;

        case 'V':
        {
            format = LogFormatLevelUppercase;
        } break;

        case 'v':
        {
            format = LogFormatLevelTitlecase;
        } break;

        case '%':
        {
            format = LogFormatPercent;
        } break;
*/
        default:
        {
            format = 0;
        }
    }

    return format;
}

internal LogFormatFnType*
LogFormatGetNextToken(const char** fmt)
{
    LogFormatFnType* format = 0;

    if (fmt)
    {
        char tokenSpecifier = **fmt;

        if (tokenSpecifier == '%')
        {
            tokenSpecifier = LogFormatAdvanceChars(fmt, 1);
            format = LogFormatGetToken(tokenSpecifier);
        }
        else
        {
            /*format = LogFormatChars;
            char* chars = LogFormatGetNextChars(fmt);
            LogFormatAppendChars(&GlobalFormats, chars);*/
        }
    }

    return format;
}

void
LogFormatSetPattern(const char* fmt)
{
    if (fmt)
    {
        int currFormatIndex = 0;

        LogFormatFnType* token = LogFormatGetNextToken(&fmt);

        while (token)
        {
            LogFormatAppendToken(&globalFormats, token, &currFormatIndex);
            token = LogFormatGetNextToken(&fmt);
        }

        globalFormats.charsIndex = 0;
    }
}

void
LogFormatMessage(LogMsg* msg)
{
    LogFormatFnType** format = globalFormats.funcs;
    umm size = globalFormats.funcsSize;

    while (*format && size--)
    {
        (*format)(msg);
        format++;
    }
}

