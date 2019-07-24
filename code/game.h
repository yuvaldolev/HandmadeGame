#if !defined(GAME_H)

#include "game_platform.h"
#include "game_for.h"
#include "game_shared.h"
#include "game_math.h"
#include "game_memory.h"
#include "game_log.h"
#include "game_tile.h"
#include "game_random.h"
#include "game_render_group.h"

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
struct bitmap_header
{
    u16 FileType;
    u32 FileSize;
    u16 Reserved;
    u16 Reserved2;
    u32 BitmapOffset;
    u32 Size;
    s32 Width;
    s32 Height;
    u16 Planes;
    u16 BitsPerPixel;
    u32 Compression;
    u32 SizeOfBitmap;
    s32 HoriziontalResolution;
    s32 VerticalResolution;
    u32 ColorsUsed;
    u32 ColorsImportant;
    
    u32 RedMask;
    u32 GreenMask;
    u32 BlueMask;
};
#pragma pack(pop)

struct loaded_bitmap
{
    u32* Pixels;
    s32 Width;
    s32 Height;
};

struct hero_bitmaps
{
    loaded_bitmap Head;
    loaded_bitmap Cape;
    loaded_bitmap Torso;
    s32 AlignX;
    s32 AlignY;
};

struct entity
{
    b32 Exists;
    tile_map_position P;
    v2 dP;
    u32 FacingDirection;
    f32 Width, Height;
};

struct world
{
    tile_map* TileMap;
};

struct game_state
{
    memory_arena LoggingArena;
    memory_arena WorldArena;
    
    world* World;
    
    tile_map_position CameraP;
    tile_map_position PlayerP;
    v2 dPlayerP;
    
    loaded_bitmap Backdrop;
    
    u32 HeroFacingDirection;
    hero_bitmaps HeroBitmaps[4];
};

#define GAME_H
#endif
