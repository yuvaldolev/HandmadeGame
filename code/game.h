#if !defined(GAME_H)

#include "game_platform.h"
#include "game_shared.h"
#include "game_math.h"
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

struct dormant_entity
{
    tile_map_position P;
    f32 Width;
    f32 Height;
};

struct low_entity
{
    
};

struct high_entity
{
    b32 Exists;
    v2 P;
    v2 dP;
    u32 FacingDirection;
};

enum entity_residence
{
    EntityResidence_NoneExistent,
    EntityResidence_Dormant,
    EntityResidence_Low,
    EntityResidence_High,
};

struct entity
{
    u32 Residence;
    
    low_entity* Low;
    high_entity* High;
    dormant_entity* Dormant;
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
    
    u32 EntityCount;
    entity_residence EntityResidence[256];
    high_entity HighEntities[256];
    low_entity LowEntities[256];
    dormant_entity DormantEntities[256];
    
    loaded_bitmap Backdrop;
    
    u32 HeroFacingDirection;
    hero_bitmaps HeroBitmaps[4];
};

#define GAME_H
#endif
