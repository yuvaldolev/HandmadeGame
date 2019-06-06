#include "game_log_format.cpp"

// TODO(yuval, eran): @Temporary
#include <stdio.h>

global_variable log_level GlobalMinLevel;

void
LogInit(memory_arena* Arena,
        log_level minLevel, const char* LogFmt)
{
    GlobalMinLevel = minLevel;
    LogFormatSetPattern(Arena, LogFmt);
}

internal void
Log(log_level Level, const char* LogFileName,
    const char* LogFnName, long LogLine,
    const char* Format, ...)
{
    if (Level >= GlobalMinLevel)
    {
        // NOTE: VA LIST extraction
        va_list ArgList;
        va_start(ArgList, Format);
        
        // NOTE: LogMsg struct instance
        const u32 FORMATTED_SIZE = 4096;
        char Formatted[FORMATTED_SIZE] = { };
        
        log_msg Msg = { };
        Msg.Level = Level;
        Msg.File = LogFileName;
        Msg.Fn = LogFnName;
        Msg.Line = LogLine;
        Msg.Format = Format;
        Msg.ArgList = &ArgList;
        Msg.Formatted = Formatted;
        Msg.FormattedAt = Formatted;
        Msg.RemainingFormattingSpace = FORMATTED_SIZE;
        Msg.MaxSize = FORMATTED_SIZE;
        
        // NOTE: Format Message
        LogFormatMessage(&Msg);
        
        // NOTE: Log Message In Color
        // TODO(yuval, eran): @Temporary
        printf("%s", Msg.Formatted);
        
        // NOTE: End VA LIST
        va_end(ArgList);
    }
}

