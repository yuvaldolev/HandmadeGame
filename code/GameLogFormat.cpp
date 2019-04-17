global_variable LogFormats GlobalFormats = { };

internal void
LogFormatAppendToken(LogFormats* formats, LogFormatFnType* token,
                     int* index)
{
    const int CHUNK_SIZE = 5;

    if (index >= formats->size)
    {
        if (formats->formatFuncs)
        {
            free(formats->formatFuncs);
        }

        formats->formatFuncs = (LogFormatFnType**)
            malloc(sizeof(LogFormatFnType*) * CHUNK_SIZE);
        formats->size += CHUNK_SIZE;
    }

    formats->formatFuncs[index] = token;
    ++(*index);
}

internal LogFormatFnType*
LogFormatGetToken(const char tokenSpecifier)
{
    LogFormatFnType* format;

    switch (tokenSpecifier)
    {
        case 'm':
        {
            format = LogFormatMessage;
        } break;

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

        default:
        {
            format = 0;
        }
    }

    return format;
}

internal char
LogFormatAdvanceChars(const char** fmt, uint32 amount)
{
    *fmt += amount;
    return **fmt;
}

internal LogFormatFnType*
LogFormatGetNextToken(const char** fmt)
{
    LogFormatFnType* format = 0;
    char tokenSpecifier = **fmt;

    if (tokenSpecifier == '%')
    {
        tokenSpecifier = LogFormatAdvanceChars(fmt, 1);
        format = LogFormatGetToken(tokenSpecifier);
    }

    return format;
}

LogFormats
LogFormatPattern(const char* fmt)
{
    if (fmt)
    {
        int currFormatIndex = 0;

        LogFormatFnType* token = LogFormatGetNextToken(&fmt);

        while (token)
        {
            LogFormatAppendToken(&GlobalFormats, token, &currFormatIndex);
            token = LogFormatGetNextToken(&fmt);
        }
    }

    return formats;
}

