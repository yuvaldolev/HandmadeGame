#if !defined(GAME_H)

#include "game_types.h"
#include "game_intrinsics.h"
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
#include "game_log.h"

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
    u32 sampleCount;
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
    DEBUGPlatformFreeFileMemoryType* DEBUGPlatformFreeFileMemory;
    DEBUGPlatformReadEntireFileType* DEBUGPlatformReadEntireFile;
    DEBUGPlatformWriteEntireFileType* DEBUGPlatformWriteEntireFile;
};

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
#pragma pack(push, 1)
struct BitmapHeader
{
    u16 fileType;
    u32 fileSize;
    u16 reserved;
    u16 reserved2;
    u32 bitmapOffset;
    u32 size;
    s32 width;
    s32 height;
    u16 planes;
    u16 bitsPerPixel;
    u32 compression;
    u32 sizeOfBitmap;
    s32 horiziontalResolution;
    s32 verticalResolution;
    u32 colorUser;
    u32 colorsImportant;
    
    u32 redMask;
    u32 greenMask;
    u32 blueMask;
};
#pragma pack(pop)

struct LoadedBitmap
{
    u32* pixels;
    s32 width;
    s32 height;
};

struct TileChunkPosition
{
    u32 tileChunkX;
    u32 tileChunkY;
    
    u32 relTileX;
    u32 relTileY;
};

struct WorldPosition
{
    // NOTE: These are fixed point tile locations.
    // The high bits are the tile chunk index,
    // and the low bits are the tile index in the chunk.
    u32 absTileX;
    u32 absTileY;
    
    // TODO(yuval, eran): Maybe @Rename these to offset X / Y
    f32 tileRelX;
    f32 tileRelY;
};

struct TileChunk
{
    u32* tiles;
};

struct World
{
    u32 chunkShift;
    u32 chunkMask;
    u32 chunkDim;
    
    f32 tileSideInMeters;
    s32 tileSideInPixels;
    f32 metersToPixels;
    
    s32 tileChunkCountX;
    s32 tileChunkCountY;
    
    TileChunk* tileChunks;
};

struct HeroBitmap
{
    LoadedBitmap head;
    LoadedBitmap cape;
    LoadedBitmap torso;
};

struct GameState
{
    WorldPosition playerP;
    
    LoadedBitmap backdrop;
    
    u32 facingDirection;
    HeroBitmap heroBitmap[4];
};

#define GAME_H
#endif
