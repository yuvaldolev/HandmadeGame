#include "game_log_format.cpp"

global_variable LogLevelEnum globalMinLevel;
global_variable GameMemory* globalLogMemory;

void
LogInit(MemoryArena* arena,
        LogLevelEnum minLevel, const char* logFmt)
{
    globalMinLevel = minLevel;
    LogFormatSetPattern(arena, logFmt);
}

extern "C" LOG(Log)
{
    if (level >= globalMinLevel)
    {
        // NOTE(yuval): VA LIST extraction
        va_list argList;
        va_start(argList, format);

        // NOTE(yuval): LogMsg struct instance
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

        // NOTE(yuval): Format Message
        LogFormatMessage(&msg);

        // NOTE(yuval): Log Message In Color
        globalLogMemory->PlatformWriteLogMsg(&msg);

        // NOTE(yuval): End VA LIST
        va_end(argList);
    }
}

