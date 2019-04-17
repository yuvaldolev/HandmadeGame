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
    const char* format;
    char* formatted;
    LogLevelEnum level;
};

#include "GameLogFormat.h"

void LogInit(LogLevelEnum minLevel, const char* logFmt);

void Log(LogLevelEnum level,
         const char* logFileName, size_t logFileLen,
         const char* logFuncName, size_t logFuncLen,
         long logLine,
         const char* format, ...);

// NOTE(yuval): This macro is internal do not use in other files!
#define LogInternal(level, format, ...) Log(level, \
    __FILE__, sizeof(__FILE__) - 1, \
    __FUNCTION__, sizeof(__FUNCTION__) - 1, \
    __LINE__, \
    format, __VA_ARGS__)

#define LogDebug(format, ...) LogInternal(LOG_LEVEL_DEBUG, format, __VA_ARGS__)
#define LogInfo(format, ...) LogInternal(LOG_LEVEL_INFO, format, __VA_ARGS__)
#define LogWarn(format, ...) LogInternal(LOG_LEVEL_WARN, format, __VA_ARGS__)
#define LogError(format, ...) LogInternal(LOG_LEVEL_ERROR, format, __VA_ARGS__)
#define LogFatal(format, ...) LogInternal(LOG_LEVEL_FATAL, format, __VA_ARGS__)

#define GAMELOG_H
#endif

