#include "game_log_format.cpp"

// TODO(yuval, eran): @Temporary
#include <stdio.h>

global_variable LogLevelEnum globalMinLevel;
global_variable GameMemory* globalLogMemory;

void
LogInit(MemoryArena* arena,
        LogLevelEnum minLevel, const char* logFmt)
{
    globalMinLevel = minLevel;
    LogFormatSetPattern(arena, logFmt);
}


internal void
Log(LogLevelEnum level, const char* logFileName,
    const char* logFnName, long logLine,
    const char* format, ...)
{
    if (level >= globalMinLevel)
    {
        // NOTE: VA LIST extraction
        va_list argList;
        va_start(argList, format);
        
        // NOTE: LogMsg struct instance
        const u32 FORMATTED_SIZE = 4096;
        char formatted[FORMATTED_SIZE] = { };
        
        LogMsg msg = { };
        msg.level = level;
        msg.file = logFileName;
        msg.fn = logFnName;
        msg.line = logLine;
        msg.format = format;
        msg.argList = &argList;
        msg.formatted = formatted;
        msg.formattedAt = formatted;
        msg.remainingFormattingSpace = FORMATTED_SIZE;
        msg.maxSize = FORMATTED_SIZE;
        
        // NOTE: Format Message
        LogFormatMessage(&msg);
        
        // NOTE: Log Message In Color
        // TODO(yuval, eran): @Temporary
        printf("%s", msg.formatted);
        
        // NOTE: End VA LIST
        va_end(argList);
    }
}

