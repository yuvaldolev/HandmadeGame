inline void
RecanonicalizeCoord(TileMap* tileMap, u32* tile, f32* tileRel)
{
    // NOTE: The tileMap is assumed to be toroidal, if you step off
    // one end you come back on the other!
    
    s32 offset = RoundF32ToS32(*tileRel / tileMap->tileSideInMeters);
    *tile += offset;
    *tileRel -= offset * tileMap->tileSideInMeters;
    
    // TODO(yuval, eran): @Fix floating point math so this can be
    // only "less then" and not "less then or equals"
    Assert(*tileRel >= -0.5 * tileMap->tileSideInMeters);
    Assert(*tileRel <= 0.5 * tileMap->tileSideInMeters);
}

internal TileMapPosition
RecanonicalizePosition(TileMap* tileMap, TileMapPosition pos)
{
    TileMapPosition result = pos;
    
    RecanonicalizeCoord(tileMap, &result.absTileX, &result.tileRelX);
    RecanonicalizeCoord(tileMap, &result.absTileY, &result.tileRelY);
    
    return result;
}

internal TileChunk*
GetTileChunk(TileMap* tileMap, u32 tileChunkX, u32 tileChunkY)
{
    TileChunk* tileChunk = 0;
    
    if ((tileChunkX >= 0 && tileChunkX < tileMap->tileChunkCountX) &&
        (tileChunkY >= 0 && tileChunkY < tileMap->tileChunkCountY))
    {
        tileChunk = &tileMap->tileChunks[tileChunkY * tileMap->tileChunkCountX +
                tileChunkX];
    }
    
    return tileChunk;
}

inline TileChunkPosition
GetChunkPositionFor(TileMap* tileMap, u32 absTileX, u32 absTileY)
{
    TileChunkPosition result;
    
    result.tileChunkX = absTileX >> tileMap->chunkShift;
    result.tileChunkY = absTileY >> tileMap->chunkShift;
    result.relTileX = absTileX & tileMap->chunkMask;
    result.relTileY = absTileY & tileMap->chunkMask;
    
    return result;
}

internal u32
GetTileValueUnchecked(TileMap* tileMap, TileChunk* tileChunk, u32 tileX, u32 tileY)
{
    Assert(tileChunk);
    Assert(tileX < tileMap->chunkDim);
    Assert(tileY < tileMap->chunkDim);
    
    u32 tileValue = tileChunk->tiles[tileY * tileMap->chunkDim + tileX];
    
    return tileValue;
}

internal b32
GetTileValue(TileMap* tileMap, TileChunk* tileChunk, u32 tileX, u32 tileY)
{
    u32 tileChunkValue = 0;
    
    if (tileChunk)
    {
        tileChunkValue = GetTileValueUnchecked(tileMap, tileChunk, tileX, tileY);
    }
    
    return tileChunkValue;
}

internal u32
GetTileValue(TileMap* tileMap, u32 absTileX, u32 absTileY)
{
    TileChunkPosition chunkPos = GetChunkPositionFor(tileMap, absTileX, absTileY);
    TileChunk* tileChunk = GetTileChunk(tileMap, chunkPos.tileChunkY, chunkPos.tileChunkX);
    u32 tileChunkValue = GetTileValue(tileMap, tileChunk, chunkPos.relTileX, chunkPos.relTileY);
    
    return tileChunkValue;
}

internal u32
SetTileValueUnchecked(TileMap* tileMap, TileChunk* tileChunk,
                      u32 tileX, u32 tileY, u32 tileValue)
{
    Assert(tileChunk);
    Assert(tileX < tileMap->chunkDim);
    Assert(tileY < tileMap->chunkDim);
    
    tileChunk->tiles[tileY * tileMap->chunkDim + tileX] = tileValue;
    
    return tileValue;
}


internal void
SetTileValue(TileMap* tileMap, TileChunk* tileChunk,
             u32 tileX, u32 tileY, u32 tileValue)
{
    if (tileChunk)
    {
        SetTileValueUnchecked(tileMap, tileChunk, tileX, tileY, tileValue);
    }
}

internal void
SetTileValue(MemoryArena* arena, TileMap* tileMap,
             u32 absTileX, u32 absTileY, u32 tileValue)
{
    TileChunkPosition chunkPos = GetChunkPositionFor(tileMap, absTileX, absTileY);
    TileChunk* tileChunk = GetTileChunk(tileMap, chunkPos.tileChunkY, chunkPos.tileChunkX);
    
    // TODO(yuval): On-demand tile chunk creation
    Assert(tileChunk);
    
    SetTileValue(tileMap, tileChunk, chunkPos.relTileX,
                 chunkPos.relTileY, tileValue);
}

internal b32
IsTileMapPointEmpty(TileMap* tileMap, TileMapPosition pos)
{
    u32 tileChunkValue = GetTileValue(tileMap, pos.absTileX, pos.absTileY);
    b32 isEmpty = (tileChunkValue == 0);
    
    return isEmpty;
}
