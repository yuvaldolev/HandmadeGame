#if !defined(GAMELOG_H)

enum LogLevelEnum
{
    LogLevelDebug = 0,
    LogLevelInfo,
    LogLevelWarn,
    LogLevelError,
    LogLevelFatal
};

struct LogMsg
{
    LogLevelEnum level;
    const char* file;
    const char* fn;
    long line;
    const char* format;
    va_list argList;
    char* formatted;
    char* formattedAt;
    umm remainingFormattingSpace;
    umm maxSize;
};

#include "game_log_format.h"

void
LogInit(MemoryArena* arena,
        LogLevelEnum minLevel, const char* logFmt);

void
LogFini();

// TODO(yuval & eran): Move & Rename this!!!!
#define LOG(name) void name(LogLevelEnum level, const char* logFileName, \
const char* logFnName, long logLine, \
const char* format, ...)
typedef LOG(LogType);
LOG(LogStub)
{
}

// NOTE(yuval): This macro is internal do not use in other files!
#define LogInternal(level, format, ...) Log(level, \
__FILE__, \
__FUNCTION__, \
__LINE__, \
format, __VA_ARGS__)

#define LogDebug(format, ...) LogInternal(LogLevelDebug, format, __VA_ARGS__)
#define LogInfo(format, ...) LogInternal(LogLevelInfo, format, __VA_ARGS__)
#define LogWarn(format, ...) LogInternal(LogLevelWarn, format, __VA_ARGS__)
#define LogError(format, ...) LogInternal(LogLevelError, format, __VA_ARGS__)
#define LogFatal(format, ...) LogInternal(LogLevelFatal, format, __VA_ARGS__)

#define GAMELOG_H
#endif

