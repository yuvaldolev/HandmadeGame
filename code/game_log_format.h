#if !defined(GAME_LOG_FORMAT_H)

internal void
LogFormatSetPattern(MemoryArena* arena, const char* fmt);

internal void
LogFormatClean();

internal void
LogFormatMessage(LogMsg* msg);

#define GAME_LOG_FORMAT_H
#endif

