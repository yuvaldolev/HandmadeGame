#if !defined(GAME_H)

#include "game_types.h"
#include "game_shared.h"
#include "game_memory.h"

/*
   NOTE(yuval): Build Options
GAME_INTERNAL:
0 - Build for public release
1 - Build for developers only

 GAME_SLOW:
 0 - No slow code is allowed
 1 - Slow code is welcome
*/

////////////////////////////////////
//          Platform API          //
////////////////////////////////////
struct ThreadContext
{
    s32 placeHolder;
};

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

#define PLATFORM_GET_DATE_TIME(name) PlatformDateTime name()
typedef PLATFORM_GET_DATE_TIME(PlatformGetDateTimeType);

// TODO(yuval & eran): Temporary!
#define PLATFORM_WRITE_LOG_MSG(name) void name(struct LogMsg* msg)
typedef PLATFORM_WRITE_LOG_MSG(PlatformWriteLogMsgType);

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

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(ThreadContext* thread, void* memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemoryType);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) DEBUGReadFileResult name(ThreadContext* thread, \
const char* fileName)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFileType);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) b32 name(ThreadContext* thread, \
const char* fileName, \
void* memory, u32 memorySize)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFileType);

#endif

// NOTE(yuval): Services that the game provides to the platform
struct GameOffscreenBuffer
{
    void* memory;
    s32 width;
    s32 height;
    s32 pitch;
    s32 bytesPerPixel;
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
    b32 isConnected;
    b32 isAnalog;
    
    f32 stickAverageX;
    f32 stickAverageY;
    
    union
    {
        GameButtonState buttons[13];
        
        struct
        {
            GameButtonState moveUp;
            GameButtonState moveDown;
            GameButtonState moveLeft;
            GameButtonState moveRight;
            
            GameButtonState run;
            
            GameButtonState actionUp;
            GameButtonState actionDown;
            GameButtonState actionLeft;
            GameButtonState actionRight;
            
            GameButtonState leftShoulder;
            GameButtonState rightShoulder;
            
            GameButtonState back;
            GameButtonState start;
            
            // IMPORTANT(yuval): Add new buttons before this ButtonState
            GameButtonState terminator;
        };
    };
};

struct GameInput
{
    f32 dtForFrame;
    
    GameButtonState mouseButtons[5];
    s32 mouseX, mouseY, mouseZ;
    
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
    
    PlatformGetDateTimeType* PlatformGetDateTime;
    PlatformWriteLogMsgType* PlatformWriteLogMsg;
    DEBUGPlatformFreeFileMemoryType* DEBUGPlatformFreeFileMemory;
    DEBUGPlatformReadEntireFileType* DEBUGPlatformReadEntireFile;
    DEBUGPlatformWriteEntireFileType* DEBUGPlatformWriteEntireFile;
};

#include "game_log.h" // TODO(yuval & eran): Move this back to the top

/* GameUpdateAndRender gets 4 thing from the Platform:
   1. Timing
   2. Controller/Keyboard input
   3. Bitmap buffer to use
   4. Sound buffer to use
*/
#define GAME_UPDATE_AND_RENDER(name) void name(ThreadContext* thread, GameMemory* memory, \
GameInput* input, \
GameOffscreenBuffer* offscreenBuffer)
typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRenderType);

#define GAME_GET_SOUND_SAMPLES(name) void name(ThreadContext* thread, GameMemory* memory, \
GameSoundOutputBuffer* soundBuffer)
typedef GAME_GET_SOUND_SAMPLES(GameGetSoundSamplesType);


/////////////////////////////
//          Game           //
/////////////////////////////
struct WorldPosition
{
    s32 tileMapX;
    s32 tileMapY;
    
    s32 tileX;
    s32 tileY;
    
    f32 tileRelX;
    f32 tileRelY;
};

struct TileMap
{
    u32* tiles;
};

struct World
{
    f32 tileSideInMeters;
    s32 tileSideInPixels;
    f32 metersToPixels;
    
    s32 tileMapCountX;
    s32 tileMapCountY;
    
    s32 tileCountX;
    s32 tileCountY;
    
    f32 upperLeftX;
    f32 upperLeftY;
    
    TileMap* tileMaps;
};

struct GameState
{
    WorldPosition playerP;
};

#define GAME_H
#endif

