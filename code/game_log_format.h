#if !defined(GAME_LOG_FORMAT_H)

void
LogFormatSetPattern(MemoryArena* arena, const char* fmt);

void
LogFormatClean();

void
LogFormatMessage(LogMsg* msg);

#define GAME_LOG_FORMAT_H
#endif

