#if !defined(GAME_LOG_FORMAT_H)

internal void
LogFormatSetPattern(memory_arena* Arena, const char* Fmt);

internal void
LogFormatClean();

internal void
LogFormatMessage(log_msg* Msg);

#define GAME_LOG_FORMAT_H
#endif

