#if !defined(GAME_H)

#include "game_types.h"
#include "game_shared.h"
#include "game_memory.h"
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

#ifdef GAME_INTERNAL
/*
IMPORTANT(yuval):
This code is NOT shipping code -
They are blocking and write donesn't protect against lost data!
*/
struct DEBUGReadFileResult
{
    void* contents;
    u32 contentsSize;
    
};

DEBUGReadFileResult
DEBUGPlatformReadEntireFile(const char* filename);

void
DEBUGPlatformFreeFileMemory(void* memory);

b32
DEBUGPlatformWriteEntireFile(const char* filename,
                             void* memory, u32 memorySize);
#endif

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

struct GameButtonState
{
    s32 halfTransitionCount;
    b32 endedDown;
};

struct GameController
{
    b32 isAnalog;
    
    r32 stickAverageX;
    r32 stickAverageY;
    
    union
    {
        GameButtonState buttons[6];
        
        struct
        {
            GameButtonState moveUp;
            GameButtonState moveDown;
            GameButtonState moveLeft;
            GameButtonState moveRight;
            
            GameButtonState actionUp;
            GameButtonState actionDown;
            GameButtonState actionLeft;
            GameButtonState actionRight;
            
            GameButtonState leftShoulder;
            GameButtonState rightShoulder;
            
            GameButtonState back;
            GameButtonState start;
        };
    };
};

struct GameInput
{
    GameController controllers[5];
};

struct GameMemory
{
    b32 isInitialized;
    
    void* permanentStorage; // NOTE(yuval): Memory is REQUIRED to be initialized to 0 at startup
    u64 permanentStorageSize;
    
    void* transientStorage; // NOTE(yuval): Memory is REQUIRED to be initialized to 0 at startup
    u64 transientStorageSize;
    
    MemoryArena loggingArena;
};

/* GameUpdateAndRender gets 4 thing from the Platform:
   1. Timing
   2. Controller/Keyboard input
   3. Bitmap buffer to use
   4. Sound buffer to use
*/
void
GameUpdateAndRender(GameMemory* memory, GameInput* input,
                    GameOffscreenBuffer* offscreenBuffer,
                    GameSoundOutputBuffer* soundBuffer);

/////////////////////////////
//          Game           //
/////////////////////////////
struct GameState
{
    s32 toneHz;
    s32 blueOffset;
    s32 greenOffset;
};

#define GAME_H
#endif

