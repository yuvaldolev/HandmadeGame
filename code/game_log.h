#if !defined(GAME_LOG_H)

enum log_level
{
    LogLevel_Debug = 0,
    LogLevel_Info,
    LogLevel_Warn,
    LogLevel_Error,
    LogLevel_Fatal
};

struct log_msg
{
    log_level Level;
    const char* File;
    const char* Fn;
    long Line;
    const char* Format;
    va_list* ArgList;
    char* Formatted;
    char* FormattedAt;
    umm RemainingFormattingSpace;
    umm MaxSize;
};

#include "game_log_format.h"

internal void
LogInit(memory_arena* Arena,
        log_level MinLevel, const char* LogFmt);

internal void
Log(log_level Level, const char* LogFileName,
    const char* LogFnName, long LogLine,
    const char* Format, ...);

// NOTE(yuval): This macro is internal do not use in other files!
#define LogInternal(Level, Format, ...) Log(Level, \
__FILE__, \
__FUNCTION__, \
__LINE__, \
Format, ##__VA_ARGS__)

#define LogDebug(Format, ...) LogInternal(LogLevelDebug, Format, ##__VA_ARGS__)
#define LogInfo(Format, ...) LogInternal(LogLevelInfo, Format, ##__VA_ARGS__)
#define LogWarn(Format, ...) LogInternal(LogLevelWarn, Format, ##__VA_ARGS__)
#define LogError(Format, ...) LogInternal(LogLevelError, Format, ##__VA_ARGS__)
#define LogFatal(Format, ...) LogInternal(LogLevelFatal, Format, ##__VA_ARGS__)

#define GAME_LOG_H
#endif

