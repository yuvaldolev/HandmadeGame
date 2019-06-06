#if !defined(GAME_TILE_H)

struct tile_map_position
{
    // NOTE: These are fixed point tile locations.
    // The high bits are the tile chunk index,
    // and the low bits are the tile index in the chunk.
    u32 AbsTileX;
    u32 AbsTileY;
    u32 AbsTileZ;
    
    // NOTE: These are the offsets from the tile center
    v2 Offset;
};

struct tile_chunk_position
{
    u32 TileChunkX;
    u32 TileChunkY;
    u32 TileChunkZ;
    
    u32 RelTileX;
    u32 RelTileY;
};

struct tile_chunk
{
    u32* Tiles;
};

struct tile_map
{
    u32 ChunkShift;
    u32 ChunkMask;
    u32 ChunkDim;
    
    f32 TileSideInMeters;
    
    u32 TileChunkCountX;
    u32 TileChunkCountY;
    u32 TileChunkCountZ;
    tile_chunk* TileChunks;
};

struct tile_map_difference
{
    v2 dXY;
    f32 dZ;
};

#define GAME_TILE_H
#endif