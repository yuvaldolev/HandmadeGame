inline void
RecanonicalizeCoord(tile_map* TileMap, u32* Tile, f32* TileRel)
{
    // NOTE: The tile map is assumed to be toroidal, if you step off
    // one end you come back on the other!
    
    s32 Offset = RoundF32ToS32(*TileRel / TileMap->TileSideInMeters);
    *Tile += Offset;
    *TileRel -= Offset * TileMap->TileSideInMeters;
    
    // TODO(yuval, eran): @Fix floating point math so this can be
    // only "less then" and not "less then or equals"
    Assert(*TileRel >= -0.5 * TileMap->TileSideInMeters);
    Assert(*TileRel <= 0.5 * TileMap->TileSideInMeters);
}

internal tile_map_position
RecanonicalizePosition(tile_map* TileMap, tile_map_position Pos)
{
    tile_map_position Result = Pos;
    
    RecanonicalizeCoord(TileMap, &Result.AbsTileX, &Result.Offset.X);
    RecanonicalizeCoord(TileMap, &Result.AbsTileY, &Result.Offset.Y);
    
    return Result;
}

internal tile_chunk*
GetTileChunk(tile_map* TileMap, u32 TileChunkX, u32 TileChunkY, u32 TileChunkZ)
{
    tile_chunk* TileChunk = 0;
    
    if ((TileChunkX >= 0 && TileChunkX < TileMap->TileChunkCountX) &&
        (TileChunkY >= 0 && TileChunkY < TileMap->TileChunkCountY) &&
        (TileChunkZ >= 0) && TileChunkZ < TileMap->TileChunkCountZ)
    {
        TileChunk =
            &TileMap->TileChunks[TileChunkZ * TileMap->TileChunkCountY * TileMap->TileChunkCountX +
                TileChunkY * TileMap->TileChunkCountX +
                TileChunkX];
    }
    
    return TileChunk;
}

inline tile_chunk_position
GetChunkPositionFor(tile_map* TileMap, u32 AbsTileX, u32 AbsTileY, u32 AbsTileZ)
{
    tile_chunk_position Result;
    
    Result.TileChunkX = AbsTileX >> TileMap->ChunkShift;
    Result.TileChunkY = AbsTileY >> TileMap->ChunkShift;
    Result.TileChunkZ = AbsTileZ;
    Result.RelTileX = AbsTileX & TileMap->ChunkMask;
    Result.RelTileY = AbsTileY & TileMap->ChunkMask;
    
    return Result;
}

internal u32
GetTileValueUnchecked(tile_map* TileMap, tile_chunk* TileChunk, u32 TileX, u32 TileY)
{
    Assert(TileChunk);
    Assert(TileX < TileMap->ChunkDim);
    Assert(TileY < TileMap->ChunkDim);
    
    u32 TileValue = TileChunk->Tiles[TileY * TileMap->ChunkDim + TileX];
    
    return TileValue;
}

internal b32
GetTileValue(tile_map* TileMap, tile_chunk* TileChunk, u32 TileX, u32 TileY)
{
    u32 TileChunkValue = 0;
    
    if (TileChunk && TileChunk->Tiles)
    {
        TileChunkValue = GetTileValueUnchecked(TileMap, TileChunk, TileX, TileY);
    }
    
    return TileChunkValue;
}

internal u32
GetTileValue(tile_map* TileMap, u32 AbsTileX, u32 AbsTileY, u32 AbsTileZ)
{
    tile_chunk_position ChunkPos = GetChunkPositionFor(TileMap, AbsTileX, AbsTileY, AbsTileZ);
    tile_chunk* TileChunk = GetTileChunk(TileMap, ChunkPos.TileChunkX,
                                         ChunkPos.TileChunkY, ChunkPos.TileChunkZ);
    u32 TileChunkValue = GetTileValue(TileMap, TileChunk, ChunkPos.RelTileX, ChunkPos.RelTileY);
    
    return TileChunkValue;
}

internal u32
GetTileValue(tile_map* TileMap, tile_map_position Pos)
{
    u32 TileChunkValue = GetTileValue(TileMap, Pos.AbsTileX,
                                      Pos.AbsTileY, Pos.AbsTileZ);
    
    return TileChunkValue;
}

internal u32
SetTileValueUnchecked(tile_map* TileMap, tile_chunk* TileChunk,
                      u32 TileX, u32 TileY, u32 TileValue)
{
    Assert(TileChunk);
    Assert(TileX < TileMap->ChunkDim);
    Assert(TileY < TileMap->ChunkDim);
    
    TileChunk->Tiles[TileY * TileMap->ChunkDim + TileX] = TileValue;
    
    return TileValue;
}


internal void
SetTileValue(tile_map* TileMap, tile_chunk* TileChunk,
             u32 TileX, u32 TileY, u32 TileValue)
{
    if (TileChunk && TileChunk->Tiles)
    {
        SetTileValueUnchecked(TileMap, TileChunk, TileX, TileY, TileValue);
    }
}

internal void
SetTileValue(memory_arena* Arena, tile_map* TileMap,
             u32 AbsTileX, u32 AbsTileY, u32 AbsTileZ,
             u32 TileValue)
{
    tile_chunk_position ChunkPos = GetChunkPositionFor(TileMap, AbsTileX, AbsTileY, AbsTileZ);
    tile_chunk* TileChunk = GetTileChunk(TileMap, ChunkPos.TileChunkX,
                                         ChunkPos.TileChunkY, ChunkPos.TileChunkZ);
    
    // TODO(yuval): On-demand tile chunk creation
    if (!TileChunk->Tiles)
    {
        u32 TileCount = TileMap->ChunkDim * TileMap->ChunkDim;
        TileChunk->Tiles = PushArray(Arena, u32, TileCount);
        
        for (u32 TileIndex = 0; TileIndex < TileCount; ++TileIndex)
        {
            TileChunk->Tiles[TileIndex] = 1;
        }
    }
    
    SetTileValue(TileMap, TileChunk, ChunkPos.RelTileX,
                 ChunkPos.RelTileY, TileValue);
}

internal b32
IsTileMapPointEmpty(tile_map* TileMap, tile_map_position Pos)
{
    u32 TileChunkValue = GetTileValue(TileMap, Pos.AbsTileX, Pos.AbsTileY, Pos.AbsTileZ);
    b32 IsEmpty = ((TileChunkValue == 1) ||
                   (TileChunkValue == 3) ||
                   (TileChunkValue == 4));
    
    return IsEmpty;
}

internal b32
AreOnSameTile(tile_map_position* A, tile_map_position* B)
{
    b32 Result = ((A->AbsTileX == B->AbsTileX) &&
                  (A->AbsTileY == B->AbsTileY) &&
                  (A->AbsTileZ == B->AbsTileZ));
    
    return Result;
}

inline tile_map_difference
Subtract(tile_map* TileMap, tile_map_position* A, tile_map_position* B)
{
    tile_map_difference Result;
    
    v2 dTileXY = { (f32)A->AbsTileX - (f32)B->AbsTileX,
        (f32)A->AbsTileY - (f32)B->AbsTileY };
    f32 dTileZ = (f32)A->AbsTileZ - (f32)B->AbsTileZ;
    
    Result.dXY = TileMap->TileSideInMeters  * dTileXY + (A->Offset - B->Offset);
    Result.dZ = dTileZ * TileMap->TileSideInMeters;
    
    return Result;
}