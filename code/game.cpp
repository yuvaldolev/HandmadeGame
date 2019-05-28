#include "game.h"

#include "game_log.cpp"

#include <math.h> // TODO(yuval & eran): Temporary

#define RGB(R, G, B) ((RoundF32ToU32(R * 255.0f) << 16) | \
(RoundF32ToU32(G * 255.0f) << 8) | \
(RoundF32ToU32(B * 255.0f)))

inline s32
RoundF32ToS32(f32 value)
{
    s32 result = (s32)(value + 0.5f);
    // TODO(yuval & eran): @Add intrinsics
    return result;
}

inline u32
RoundF32ToU32(f32 value)
{
    u32 result = (u32)(value + 0.5f);
    // TODO(yuval & eran): @Add intrinsics
    return result;
}

inline s32
FloorF32ToS32(f32 value)
{
    // TODO(yuval, eran): Implement floorf function ourselves
    s32 result = (s32)floorf(value);
    return result;
}

internal u32
GetTileValueUnchecked(World* world, TileMap* tileMap, s32 tileX, s32 tileY)
{
    u32 tileValue = tileMap->tiles[tileY * world->tileCountX + tileX];
    return tileValue;
}

internal TileMap*
GetTileMap(World* world, s32 tileMapX, s32 tileMapY)
{
    TileMap* tileMap = 0;
    
    if ((tileMapX >= 0 && tileMapX < world->tileMapCountX) &&
        (tileMapY >= 0 && tileMapY < world->tileMapCountY))
    {
        tileMap = &world->tileMaps[tileMapY * world->tileMapCountX +
                tileMapX];
    }
    
    return tileMap;
}

inline void
RecanonicalizeCoord(World* world, s32 tileCount, s32* tileMap, s32* tile, f32* tileRel)
{
    s32 offset = FloorF32ToS32(*tileRel / world->tileSideInMeters);
    *tile += offset;
    *tileRel -= offset * world->tileSideInMeters;
    
    Assert(*tileRel >= 0);
    Assert(*tileRel < world->tileSideInPixels);
    
    if (*tile < 0)
    {
        *tile = tileCount + *tile;
        --(*tileMap);
    }
    
    if (*tile >= tileCount)
    {
        *tile = *tile - tileCount;
        ++(*tileMap);
    }
}

internal CanonicalPosition
RecanonicalizePosition(World* world, CanonicalPosition pos)
{
    CanonicalPosition result = pos;
    
    RecanonicalizeCoord(world, world->tileCountX, &result.tileMapX, &result.tileX, &result.tileRelX);
    RecanonicalizeCoord(world, world->tileCountY, &result.tileMapY, &result.tileY, &result.tileRelY);
    
    return result;
}


internal b32
IsTileMapPointEmpty(World* world, TileMap* tileMap,
                    s32 tileX, s32 tileY)
{
    b32 isEmpty = false;
    
    if (tileMap)
    {
        if (tileX >= 0 && tileX < world->tileCountX &&
            tileY >= 0 && tileY < world->tileCountY)
        {
            u32 tileValue = GetTileValueUnchecked(world, tileMap,
                                                  tileX, tileY);
            isEmpty = (tileValue == 0);
        }
    }
    
    return isEmpty;
}

internal b32
IsWorldPointEmpty(World* world, CanonicalPosition pos)
{
    TileMap* tileMap = GetTileMap(world, pos.tileMapX, pos.tileMapY);
    b32 isEmpty = IsTileMapPointEmpty(world, tileMap, pos.tileX, pos.tileY);
    
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
    
    for (s32 sampleIndex = 0;
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
        
        gameState->playerP.tileMapX = 0;
        gameState->playerP.tileMapY = 0;
        gameState->playerP.tileX = 3;
        gameState->playerP.tileY = 3;
        gameState->playerP.tileRelX = 5.0f;
        gameState->playerP.tileRelY = 5.0f;
        
        memory->isInitialized = true;
    }
    
    World world;
    
    world.tileSideInMeters = 1.4f;
    world.tileSideInPixels = 60;
    world.metersToPixels = (f32)world.tileSideInPixels / world.tileSideInMeters;
    
    world.tileMapCountX = 2;
    world.tileMapCountY = 2;
    
    world.tileCountX = 17;
    world.tileCountY = 9;
    
    world.upperLeftX = -(f32)world.tileSideInPixels / 2;
    world.upperLeftY = 0;
    
    f32 playerHeight = 1.4f;
    f32 playerWidth = playerHeight * 0.75f;
    
    u32 tiles00[9][17] = {
        { 1, 1, 1, 1,  1, 1, 1, 1,  1,  1, 1, 1, 1,  1, 1, 1, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  1, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 0 },
        { 1, 0, 1, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 1, 1, 1,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1, 1 }
    };
    
    u32 tiles01[9][17] = {
        { 1, 1, 1, 1,  1, 1, 1, 1,  1,  1, 1, 1, 1,  1, 1, 1, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 1, 1 },
        { 1, 0, 1, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  1, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 0, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 1, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  1, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  1, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 1, 1, 1,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1, 1 }
    };
    u32 tiles10[9][17] = {
        { 1, 1, 1, 1,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 0 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 1, 1, 1,  1, 1, 1, 1,  1,  1, 1, 1, 1,  1, 1, 1, 1 }
        
    };
    u32 tiles11[9][17] = {
        { 1, 1, 1, 1,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 1, 0,  0, 0, 0, 0,  0,  1, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  1, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 0, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  1, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 1, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 1, 1, 1,  1, 1, 1, 1,  1,  1, 1, 1, 1,  1, 1, 1, 1 }
    };
    
    TileMap tileMaps[2][2];
    
    tileMaps[0][0].tiles = (u32*)tiles00;
    tileMaps[0][1].tiles = (u32*)tiles01;
    tileMaps[1][0].tiles = (u32*)tiles10;
    tileMaps[1][1].tiles = (u32*)tiles11;
    
    world.tileMaps = (TileMap*)tileMaps;
    
    TileMap* tileMap = GetTileMap(&world, gameState->playerP.tileMapX,
                                  gameState->playerP.tileMapY);
    
    for (s32 controllerIndex = 0;
         controllerIndex < ArrayCount(input->controllers);
         ++controllerIndex)
    {
        GameController* controller = &input->controllers[controllerIndex];
        
        if (controller->isConnected)
        {
            if (controller->isAnalog)
            {
                // TODO(yuval, eran): Analog controller tuning
            }
            else
            {
                // TODO(yuval, eran): Digital controller tuning
                
                // NOTE: Delta coordinates are pixels per second and not pixels
                f32 dPlayerX = 0.0f;
                f32 dPlayerY = 0.0f;
                
                if (controller->moveUp.endedDown)
                {
                    dPlayerY = -1.0f;
                }
                
                if (controller->moveDown.endedDown)
                {
                    dPlayerY = 1.0f;
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
                
                CanonicalPosition newPlayerP = gameState->playerP;
                newPlayerP.tileRelX += dPlayerX * input->dtForFrame;
                newPlayerP.tileRelY += dPlayerY * input->dtForFrame;
                newPlayerP = RecanonicalizePosition(&world, newPlayerP);
                
                CanonicalPosition playerLeft = newPlayerP;
                playerLeft.tileRelX -= 0.5f * playerWidth;
                playerLeft = RecanonicalizePosition(&world, playerLeft);
                
                CanonicalPosition playerRight = newPlayerP;
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
    
    for (s32 row = 0; row < world.tileCountY; ++row)
    {
        for (s32 column = 0; column < world.tileCountX; ++column)
        {
            u32 tileID = GetTileValueUnchecked(&world, tileMap, column, row);
            f32 tileColor = 0.5f;
            
            if (tileID == 1)
            {
                tileColor = 1.0f;
            }
            
            if ((column == gameState->playerP.tileX) &&
                (row == gameState->playerP.tileY))
            {
                tileColor = 0.0f;
            }
            
            f32 minX = (world.upperLeftX + (f32)(column * world.tileSideInPixels));
            f32 minY = (world.upperLeftY + (f32)(row * world.tileSideInPixels));
            f32 maxX = (f32)(minX + world.tileSideInPixels);
            f32 maxY = (f32)(minY + world.tileSideInPixels);
            
            DrawRectangle(offscreenBuffer,
                          minX, minY, maxX, maxY,
                          tileColor, tileColor, tileColor);
        }
    }
    
    f32 playerR = 1.0f;
    f32 playerG = 1.0f;
    f32 playerB = 0.0f;
    
    f32 playerLeft = world.upperLeftX + world.tileSideInPixels * gameState->playerP.tileX +
        gameState->playerP.tileRelX * world.metersToPixels -
        playerWidth * world.metersToPixels * 0.5f;
    f32 playerTop = world.upperLeftY + world.tileSideInPixels * gameState->playerP.tileY +
        gameState->playerP.tileRelY * world. metersToPixels -
        playerHeight * world.metersToPixels;
    
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
