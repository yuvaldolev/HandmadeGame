#if !defined(GAMEH)

#include "GameTypes.h"
#include "GameShared.h"
#include "GameLog.h"

struct PlatformDateTime
{
    u16 day;
    u16 month;
    u16 year;

    u16 hour;
    u16 minute;
    u16 second;
    u16 milliseconds;
};

PlatformDateTime
PlatformGetDateTime();

// TODO(yuval & eran): Temporary!
void
PlatformWriteLogMsgInColor(LogMsg* msg);

#define GAMEH
#endif

