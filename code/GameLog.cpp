#include "GameLogFormat.cpp"

global_variable LogLevelEnum GlobalMinLevel;

void
LogInit(LogLevelEnum minLevel, const char* logFmt)
{
    GlobalMinLevel = minLevel;
    LogFormatPattern(logFmt);
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
        // NOTE(yuval): Format Message

        // NOTE(yuval): Log Message In Color
    }
}

