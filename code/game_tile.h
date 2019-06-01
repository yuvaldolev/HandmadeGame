#if !defined(GAME_TILE_H)

struct TileMapPosition
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

struct TileChunkPosition
{
    u32 tileChunkX;
    u32 tileChunkY;
    
    u32 relTileX;
    u32 relTileY;
};

struct TileChunk
{
    u32* tiles;
};

struct TileMap
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

#define GAME_TILE_H
#endif