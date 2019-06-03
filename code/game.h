#if !defined(GAME_H)

#include "game_platform.h"
#include "game_memory.h"
#include "game_log.h"
#include "game_tile.h"
#include "game_random.h"

/*
   NOTE(yuval): Build Options
GAME_INTERNAL:
0 - Build for public release
1 - Build for developers only

 GAME_SLOW:
 0 - No slow code is allowed
 1 - Slow code is welcome
*/

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
    u32 colorsUsed;
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

struct HeroBitmaps
{
    LoadedBitmap head;
    LoadedBitmap cape;
    LoadedBitmap torso;
};

struct World
{
    TileMap* tileMap;
};

struct GameState
{
    MemoryArena loggingArena;
    MemoryArena worldArena;
    
    World* world;
    TileMapPosition playerP;
    
    LoadedBitmap backdrop;
    
    u32 facingDirection;
    HeroBitmaps heroBitmaps[4];
};

#define GAME_H
#endif
