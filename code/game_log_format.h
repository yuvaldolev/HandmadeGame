#if !defined(GAMELOGFORMAT_H)

void
LogFormatSetPattern(const char* fmt);

void
LogFormatClean();

void
LogFormatMessage(LogMsg* msg);

#define GAMELOGFORMAT_H
#endif

