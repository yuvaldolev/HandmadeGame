#include "GameLogFormat.cpp"

#include <stdio.h>

global_variable LogLevelEnum GlobalMinLevel;

void
LogInit(LogLevelEnum minLevel, const char* logFmt)
{
    GlobalMinLevel = minLevel;
    LogFormatSetPattern(logFmt);
}

void
Log(LogLevelEnum level,
         const char* logFileName, size_t logFileLen,
         const char* logFuncName, size_t logFuncLen,
         long logLine,
         const char* format, ...)
{
    if (level >= GlobalMinLevel)
    {
        // NOTE(yuval): VA LIST extraction
        va_list argList;
        va_start(argList, format);

        // NOTE(yuval): LogMsg struct instance
        const u32 FORMATTED_SIZE = 4096;
        char formatted[FORMATTED_SIZE] = { };

        LogMsg msg = { };
        msg.level = level;
        msg.logFileName = logFileName;
        msg.logFileLen = logFileLen;
        msg.logFuncName = logFuncName;
        msg.logFuncLen = logFuncLen;
        msg.logLine = logLine;
        msg.format = format;
        msg.argList = argList;
        msg.formatted = formatted;
        msg.formattedAt = formatted;
        msg.remainingFormattingSpace = FORMATTED_SIZE;

        // NOTE(yuval): Format Message
        LogFormatMessage(&msg);

        // NOTE(yuval): Log Message In Color
        printf("%s\n", msg.formatted);

        // NOTE(yuval): End VA LIST
        va_end(argList);
    }
}

