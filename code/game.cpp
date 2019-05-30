#include "game.h"

#include "game_log.cpp"

#include <math.h> // TODO(yuval & eran): Temporary

#define RGB(R, G, B) ((RoundF32ToU32(R * 255.0f) << 16) | \
(RoundF32ToU32(G * 255.0f) << 8) | \
(RoundF32ToU32(B * 255.0f)))

inline void
RecanonicalizeCoord(World* world, u32* tile, f32* tileRel)
{
    // NOTE: The world is assumed to be toroidal, if you step off
    // one end you come back on the other!
    
    s32 offset = FloorF32ToS32(*tileRel / world->tileSideInMeters);
    *tile += offset;
    *tileRel -= offset * world->tileSideInMeters;
    
    Assert(*tileRel >= 0);
    // TODO(yuval, eran): @Fix floating point math so this can be
    // only "less then" and not "less then or equals"
    Assert(*tileRel <= world->tileSideInPixels);
}

internal WorldPosition
RecanonicalizePosition(World* world, WorldPosition pos)
{
    WorldPosition result = pos;
    
    RecanonicalizeCoord(world, &result.absTileX, &result.tileRelX);
    RecanonicalizeCoord(world, &result.absTileY, &result.tileRelY);
    
    return result;
}

internal u32
GetTileValueUnchecked(World* world, TileChunk* tileChunk, u32 tileX, u32 tileY)
{
    Assert(tileChunk);
    Assert(tileX < world->chunkDim);
    Assert(tileY < world->chunkDim);
    
    u32 tileValue = tileChunk->tiles[tileY * world->chunkDim + tileX];
    return tileValue;
}

internal TileChunk*
GetTileChunk(World* world, s32 tileChunkX, s32 tileChunkY)
{
    TileChunk* tileChunk = 0;
    
    if ((tileChunkX >= 0 && tileChunkX < world->tileChunkCountX) &&
        (tileChunkY >= 0 && tileChunkY < world->tileChunkCountY))
    {
        tileChunk = &world->tileChunks[tileChunkY * world->tileChunkCountX +
                tileChunkX];
    }
    
    return tileChunk;
}

inline TileChunkPosition
GetChunkPositionFor(World* world, u32 absTileX, u32 absTileY)
{
    TileChunkPosition result;
    
    result.tileChunkX = absTileX >> world->chunkShift;
    result.tileChunkY = absTileY >> world->chunkShift;
    result.relTileX = absTileX & world->chunkMask;
    result.relTileY = absTileY & world->chunkMask;
    
    return result;
}

internal b32
GetTileValue(World* world, TileChunk* tileChunk, u32 tileX, u32 tileY)
{
    u32 tileChunkValue = 0;
    
    if (tileChunk)
    {
        tileChunkValue = GetTileValueUnchecked(world, tileChunk, tileX, tileY);
    }
    
    return tileChunkValue;
}

internal u32
GetTileValue(World* world, u32 absTileX, u32 absTileY)
{
    TileChunkPosition chunkPos = GetChunkPositionFor(world, absTileX, absTileY);
    TileChunk* tileChunk = GetTileChunk(world, chunkPos.tileChunkY, chunkPos.tileChunkX);
    u32 tileChunkValue = GetTileValue(world, tileChunk, chunkPos.relTileX, chunkPos.relTileY);
    
    return tileChunkValue;
}

internal b32
IsWorldPointEmpty(World* world, WorldPosition pos)
{
    u32 tileChunkValue = GetTileValue(world, pos.absTileX, pos.absTileY);
    b32 isEmpty = (tileChunkValue == 0);
    
    return isEmpty;
}

internal void
DrawRectangle(GameOffscreenBuffer* buffer,
              f32 realMinX, f32 realMinY,
              f32 realMaxX, f32 realMaxY,
              f32 R, f32 G, f32 B)
{
    s32 minX = RoundF32ToS32(realMinX);
    s32 minY = RoundF32ToS32(realMinY);
    s32 maxX = RoundF32ToS32(realMaxX);
    s32 maxY = RoundF32ToS32(realMaxY);
    
    if (minX < 0)
    {
        minX = 0;
    }
    
    if (minY < 0)
    {
        minY = 0;
    }
    
    if (maxX > buffer->width)
    {
        maxX = buffer->width;
    }
    
    if (maxY > buffer->height)
    {
        maxY = buffer->height;
    }
    
    u32 color = RGB(R, G, B);
    
    u8* row = ((u8*)buffer->memory +
               minX * buffer->bytesPerPixel +
               minY * buffer->pitch);
    
    for (s32 Y = minY; Y < maxY; ++Y)
    {
        u32* pixel = (u32*)row;
        
        for (s32 X = minX; X < maxX; ++X)
        {
            *pixel++ = color;
        }
        
        row += buffer->pitch;
    }
}

internal void
GameOutputSound(GameState* gameState, GameSoundOutputBuffer* buffer, const s32 toneHz)
{
    const s16 TONE_VOLUME = 10000;
    const s32 WAVE_PERIOD = buffer->samplesPerSecond / toneHz;
    
    s16* sampleOut = (s16*)buffer->samples;
    
    for (u32 sampleIndex = 0;
         sampleIndex < buffer->sampleCount;
         ++sampleIndex)
    {
#if 0
        f32 sineValue = sinf(gameState->tSine);
        s16 sampleValue = (s16)(sineValue * TONE_VOLUME);
#else
        s16 sampleValue = 0;
#endif
        *sampleOut++ = sampleValue;
        *sampleOut++ = sampleValue;
        
#if 0
        gameState->tSine += 2.0f * Pi32 * (1.0f / (f32)WAVE_PERIOD);
        
        if (gameState->tSine > 2.0f * Pi32)
        {
            gameState->tSine -= 2.0f * Pi32;
        }
#endif
    }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert((&input->controllers[0].terminator - &input->controllers[0].buttons[0]) ==
           ArrayCount(input->controllers[0].buttons));
    
    Assert(sizeof(GameMemory) <= memory->permanentStorageSize);
    
    GameState* gameState = (GameState*)memory->permanentStorage;
    
    // TODO(yuval & eran): This is temporary
    globalLogMemory = memory;
    globalLogFormatMemory = memory;
    
    if (!memory->isInitialized)
    {
        InitializeArena(&memory->loggingArena,
                        (u8*)memory->permanentStorage + sizeof(GameState),
                        (u64)(memory->permanentStorageSize * 0.0001f));
        
        LogInit(&memory->loggingArena, LogLevelDebug, "[%V] [%d] %f:%U:%L - %m%n");
        
        gameState->playerP.absTileX = 3;
        gameState->playerP.absTileY = 3;
        gameState->playerP.tileRelX = 5.0f;
        gameState->playerP.tileRelY = 5.0f;
        
        memory->isInitialized = true;
    }
    
#define TILE_CHUNK_DIM 256
    
    u32 tempTiles[TILE_CHUNK_DIM][TILE_CHUNK_DIM] = {
        { 1, 1, 1, 1,  1, 1, 1, 1,  1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1,  1, 1, 1, 1,  1, 1, 1, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 1, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1,  1, 0, 1, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  1, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1,  1, 0, 0, 0,  1, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 1, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1,  1, 0, 1, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  1, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0,  1, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 1, 1, 1,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1, 1 },
        
        { 1, 1, 1, 1,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1,  1, 0, 1, 0,  0, 0, 0, 0,  0,  1, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1,  1, 0, 0, 0,  1, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  1, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1,  1, 0, 1, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 1, 1, 1,  1, 1, 1, 1,  1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1,  1, 1, 1, 1,  1, 1, 1, 1 }
    };
    
    World world;
    
    // NOTE: This is set to be using 256x256 tile chunks
    world.chunkShift = 8;
    world.chunkMask = (1 << world.chunkShift) - 1;
    world.chunkDim = TILE_CHUNK_DIM;
    
    world.tileChunkCountX = 1;
    world.tileChunkCountY = 1;
    
    TileChunk tileChunk;
    tileChunk.tiles = (u32*)tempTiles;
    world.tileChunks = &tileChunk;
    
    world.tileSideInMeters = 1.4f;
    world.tileSideInPixels = 60;
    world.metersToPixels = (f32)world.tileSideInPixels / world.tileSideInMeters;
    
    f32 playerHeight = 1.4f;
    f32 playerWidth = playerHeight * 0.75f;
    
    f32 lowerLeftX = -(f32)world.tileSideInPixels / 2;
    f32 lowerLeftY = (f32)offscreenBuffer->height;
    
    for (u32 controllerIndex = 0;
         controllerIndex < ArrayCount(input->controllers);
         ++controllerIndex)
    {
        GameController* controller = &input->controllers[controllerIndex];
        
        if (controller->isConnected)
        {
            if (controller->isAnalog)
            {
                // TODO(yuval, eran): Analog controller tuning
                f32 dPlayerX = controller->stickAverageX;
                f32 dPlayerY = controller->stickAverageY;
                
                if (controller->run.endedDown)
                {
                    dPlayerX *= 20.0f;
                    dPlayerY *= 20.0f;
                }
                else
                {
                    dPlayerX *= 3.0f;
                    dPlayerY *= 3.0f;
                }
                
                WorldPosition newPlayerP = gameState->playerP;
                newPlayerP.tileRelX += dPlayerX * input->dtForFrame;
                newPlayerP.tileRelY += dPlayerY * input->dtForFrame;
                newPlayerP = RecanonicalizePosition(&world, newPlayerP);
                
                WorldPosition playerLeft = newPlayerP;
                playerLeft.tileRelX -= 0.5f * playerWidth;
                playerLeft = RecanonicalizePosition(&world, playerLeft);
                
                WorldPosition playerRight = newPlayerP;
                playerRight.tileRelX += 0.5f * playerWidth;
                playerRight = RecanonicalizePosition(&world, playerRight);
                
                if (IsWorldPointEmpty(&world, newPlayerP) &&
                    IsWorldPointEmpty(&world, playerLeft) &&
                    IsWorldPointEmpty(&world, playerRight))
                {
                    gameState->playerP = newPlayerP;
                }
            }
            else
            {
                // TODO(yuval, eran): Digital controller tuning
                
                // NOTE: Delta coordinates are pixels per second and not pixels
                f32 dPlayerX = 0.0f;
                f32 dPlayerY = 0.0f;
                
                if (controller->moveUp.endedDown)
                {
                    dPlayerY = 1.0f;
                }
                
                if (controller->moveDown.endedDown)
                {
                    dPlayerY = -1.0f;
                }
                
                if (controller->moveLeft.endedDown)
                {
                    dPlayerX = -1.0f;
                }
                
                if (controller->moveRight.endedDown)
                {
                    dPlayerX = 1.0f;
                }
                
                if (controller->run.endedDown)
                {
                    dPlayerX *= 6.0f;
                    dPlayerY *= 6.0f;
                }
                else
                {
                    dPlayerX *= 3.0f;
                    dPlayerY *= 3.0f;
                }
                
                WorldPosition newPlayerP = gameState->playerP;
                newPlayerP.tileRelX += dPlayerX * input->dtForFrame;
                newPlayerP.tileRelY += dPlayerY * input->dtForFrame;
                newPlayerP = RecanonicalizePosition(&world, newPlayerP);
                
                WorldPosition playerLeft = newPlayerP;
                playerLeft.tileRelX -= 0.5f * playerWidth;
                playerLeft = RecanonicalizePosition(&world, playerLeft);
                
                WorldPosition playerRight = newPlayerP;
                playerRight.tileRelX += 0.5f * playerWidth;
                playerRight = RecanonicalizePosition(&world, playerRight);
                
                if (IsWorldPointEmpty(&world, newPlayerP) &&
                    IsWorldPointEmpty(&world, playerLeft) &&
                    IsWorldPointEmpty(&world, playerRight))
                {
                    gameState->playerP = newPlayerP;
                }
            }
        }
    }
    
    DrawRectangle(offscreenBuffer, 0.0f, 0.0f,
                  (f32)offscreenBuffer->width, (f32)offscreenBuffer->height,
                  1.0f, 0.0f, 0.0f);
    
    f32 centerX = (f32)offscreenBuffer->width / 2.0f;
    f32 centerY  = (f32)offscreenBuffer->height / 2.0f;
    
    for (s32 relRow = -10; relRow < 10; ++relRow)
    {
        for (s32 relColumn = -20; relColumn < 20; ++relColumn)
        {
            u32 column = gameState->playerP.absTileX + relColumn;
            u32 row =  gameState->playerP.absTileY + relRow;
            u32 tileID = GetTileValue(&world, column, row);
            
            f32 tileColor = 0.5f;
            
            if (tileID == 1)
            {
                tileColor = 1.0f;
            }
            
            if ((column == gameState->playerP.absTileX) &&
                (row == gameState->playerP.absTileY))
            {
                tileColor = 0.0f;
            }
            
            f32 minX = centerX - gameState->playerP.tileRelX * world.metersToPixels +
                (f32)(relColumn * world.tileSideInPixels);
            f32 minY = centerY + gameState->playerP.tileRelY * world.metersToPixels -
                (f32)(relRow * world.tileSideInPixels);
            f32 maxX = (f32)(minX + world.tileSideInPixels);
            f32 maxY = (f32)(minY - world.tileSideInPixels);
            
            DrawRectangle(offscreenBuffer,
                          minX, maxY, maxX, minY,
                          tileColor, tileColor, tileColor);
        }
    }
    
    f32 playerR = 1.0f;
    f32 playerG = 1.0f;
    f32 playerB = 0.0f;
    
    f32 playerLeft = centerX - 0.5f * playerWidth * world.metersToPixels;
    
    f32 playerTop = centerY - playerHeight * world.metersToPixels;
    
    DrawRectangle(offscreenBuffer,
                  playerLeft, playerTop,
                  playerLeft + playerWidth * world.metersToPixels,
                  playerTop + playerHeight * world.metersToPixels,
                  playerR, playerG, playerB);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    GameState* gameState = (GameState*)memory->permanentStorage;
    GameOutputSound(gameState, soundBuffer, 1);
}

internal void
RenderGradient(GameOffscreenBuffer* buffer, s32 xOffset, s32 yOffset)
{
    u8* row = (u8*)buffer->memory;
    
    for (s32 y = 0; y < buffer->height; ++y)
    {
        u32* pixel = (u32*)row;
        
        for (s32 x = 0; x < buffer->width; ++x)
        {
            u8 Blue = (u8)(x + xOffset);
            u8 Green = (u8)(y + yOffset);
            
            *pixel++ = (Green << 16) | Blue;
        }
        
        row += buffer->pitch;
    }
}
