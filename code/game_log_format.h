#if !defined(GAMELOGFORMAT_H)

void
LogFormatSetPattern(MemoryArena* arena, const char* fmt);

void
LogFormatClean();

void
LogFormatMessage(LogMsg* msg);

#define GAMELOGFORMAT_H
#endif

