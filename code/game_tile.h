#if !defined(GAME_TILE_H)

struct TileMapPosition
{
    // NOTE: These are fixed point tile locations.
    // The high bits are the tile chunk index,
    // and the low bits are the tile index in the chunk.
    u32 absTileX;
    u32 absTileY;
    u32 absTileZ;
    
    // NOTE: These are the offsets from the tile center
    f32 offsetX;
    f32 offsetY;
};

struct TileChunkPosition
{
    u32 tileChunkX;
    u32 tileChunkY;
    u32 tileChunkZ;
    
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
    
    u32 tileChunkCountX;
    u32 tileChunkCountY;
    u32 tileChunkCountZ;
    TileChunk* tileChunks;
};

#define GAME_TILE_H
#endif