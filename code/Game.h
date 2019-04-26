#if !defined(GAMEH)

#include "game_types.h"
#include "game_shared.h"
#include "game_log.h"

// NOTE(yuval): Services that the platform provides to the game
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

// NOTE(yuval): Services that the game provides to the platform
struct GameOffscreenBuffer
{
    void* memory;
    s32 width;
    s32 height;
    s32 pitch;
};

struct GameSoundOutputBuffer
{
    s16* samples;
    s32 samplesPerSecond;
    s32 sampleCount;
};

/* GameUpdateAndRender gets 4 thing from the Platform:
   1. Timing
   2. Controller/Keyboard input
   3. Bitmap buffer to use
   4. Sound buffer to use
*/
void
GameUpdateAndRender(GameOffscreenBuffer* buffer, GameSoundOutputBuffer* soundBuffer);

#define GAMEH
#endif

