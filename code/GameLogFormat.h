#if !defined(GAMELOGFORMAT_H)

struct LogFormats
{
    LogFormatFnType** formatFuncs;
    size_t size;
}

#define LOG_FORMAT_FN(name) void name(LogMsg* msg, const char* format, va_list args)
typedef LOG_FORMAT_FN(LogFormatFnType);

LOG_FORMAT_FN(LogFormatMessage);
LOG_FORMAT_FN(LogFormatNewLine);
LOG_FORMAT_FN(LogFormatDate);
LOG_FORMAT_FN(LogFormatFullFileName);
LOG_FORMAT_FN(LogFormatFileName);
LOG_FORMAT_FN(LogFormatFuncName);
LOG_FORMAT_FN(LogFormatLineNum);
LOG_FORMAT_FN(LogFormatLevelUppercase);
LOG_FORMAT_FN(LogFormatLevelTitlecase);
LOG_FORMAT_FN(LogFormatPercent);
LOG_FORMAT_FN(LogFormatChar);

LogFormatFnType** LogFormatPattern(const char* fmt);

#define GAMELOGFORMAT_H
#endif

