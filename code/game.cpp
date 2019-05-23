#include "game.h"

#include "game_log.cpp"

#include <math.h> // TODO(yuval & eran): Temporary

#define RGB(R, G, B) ((RoundF32ToU32(R * 255.0f) << 16) | \
(RoundF32ToU32(G * 255.0f) << 8) | \
(RoundF32ToU32(B * 255.0f)))

internal s32
RoundF32ToS32(f32 value)
{
    s32 result = (s32)(value + 0.5f);
    // TODO(yuval & eran): @Add intrinsics
    return result;
}

internal u32
RoundF32ToU32(f32 value)
{
    u32 result = (u32)(value + 0.5f);
    // TODO(yuval & eran): @Add intrinsics
    return result;
}

internal s32
FloorF32ToS32(f32 value)
{
    // TODO(yuval, eran): Implement floorf function ourselves
    s32 result = (s32)floorf(value);
    return result;
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

internal u32
GetTileValueUnchecked(World* world, TileMap* tileMap, s32 tileX, s32 tileY)
{
    Assert(tileMap);
    Assert((tileX >= 0 && tileX < world->tileCountX) &&
           (tileY >= 0 && tileY < world->tileCountY));
    
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
        tileMap = &world->tileMaps[(tileMapY * world->tileMapCountX +
                                    tileMapX)];
    }
    
    return tileMap;
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


internal CanonicalPosition
GetCanonicalPosition(World* world, RawPosition pos)
{
    CanonicalPosition result;
    
    
    result.tileMapX = pos.tileMapX;
    result.tileMapY = pos.tileMapY;
    
    f32 X = pos.X - world->upperLeftX;
    f32 Y = pos.Y - world->upperLeftY;
    result.tileX = FloorF32ToS32(X / world->tileWidth);
    result.tileY = FloorF32ToS32(Y / world->tileHeight);
    
    
    result.X = X - result.tileX * world->tileWidth;
    result.Y = Y - result.tileY * world->tileHeight;
    
    if (result.tileX < 0)
    {
        result.tileX = world->tileCountX + result.tileX;
        --result.tileMapX;
    }
    if (result.tileY < 0)
    {
        result.tileY = world->tileCountY + result.tileY;
        --result.tileMapY;
    }
    
    if (result.tileX >= world->tileCountX)
    {
        result.tileX =  result.tileX - world->tileCountX;
        ++result.tileMapX;
    }
    
    if (result.tileY >= world->tileCountY)
    {
        result.tileY =  result.tileY - world->tileCountY;
        ++result.tileMapY;
    }
    
    return result;
}


internal b32
IsWorldPointEmpty(World* world, RawPosition pos)
{
    CanonicalPosition canPos = GetCanonicalPosition(world, pos);
    TileMap* tileMap = GetTileMap(world, canPos.tileMapX, canPos.tileMapY);
    
    b32 isEmpty = IsTileMapPointEmpty(world, tileMap, canPos.tileX, canPos.tileY);
    
    return isEmpty;
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
        
        gameState->playerX = 100.0f;
        gameState->playerY = 100.0f;
        gameState->playerTileMapX = 0;
        gameState->playerTileMapY = 0;
        
        memory->isInitialized = true;
    }
    
    World world;
    
    world.tileMapCountX = 2;
    world.tileMapCountY = 2;
    
    world.tileCountX = 17;
    world.tileCountY = 9;
    
    world.tileWidth = 60;
    world.tileHeight = 60;
    
    world.upperLeftX = -38;
    world.upperLeftY = 0;
    
    f32 playerWidth = world.tileWidth * 0.75f;
    f32 playerHeight = (f32)world.tileHeight;
    
    
    u32 tiles00[9][17] = {
        { 1, 1, 1, 1,  1, 1, 1, 1,  1,  1, 1, 1, 1,  1, 1, 1,1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 1,1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0,1 },
        { 1, 0, 0, 0,  1, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0,1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0,0 },
        { 1, 0, 1, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  1, 0, 0,1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0,1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  1, 0, 0, 0,  0, 0, 0,1 },
        { 1, 1, 1, 1,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1,1 }
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
        { 1, 0, 0, 0,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1, 1 }
    };
    u32 tiles10[9][17] = {
        { 1, 1, 1, 1,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 1, 1 },
        { 1, 0, 1, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  1, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 0 },
        { 1, 0, 1, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  1, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  1, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  1, 1, 1, 1,  1,  1, 1, 1, 1,  1, 1, 1, 1 }
        
    };
    u32 tiles11[9][17] = {
        { 1, 1, 1, 1,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 1, 1 },
        { 1, 0, 1, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  1, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 0, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 1, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  1, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  0,  1, 0, 0, 0,  0, 0, 0, 1 },
        { 1, 0, 0, 0,  1, 1, 1, 1,  1,  1, 1, 1, 1,  1, 1, 1, 1 }
    };
    
    TileMap tileMaps[2][2];
    
    tileMaps[0][0].tiles = (u32*)tiles00;
    tileMaps[0][1].tiles = (u32*)tiles01;
    tileMaps[1][0].tiles = (u32*)tiles10;
    tileMaps[1][1].tiles = (u32*)tiles11;
    
    world.tileMaps = (TileMap*)tileMaps;
    
    for (s32 controllerIndex = 0;
         controllerIndex < ArrayCount(input->controllers);
         ++controllerIndex)
    {
        GameController* controller = &input->controllers[controllerIndex];
        
        if (controller->isConnected)
        {
            if (controller->isAnalog)
            {
                // TODO(yuval & eran): Analog controller tuning
                
                //gameState->blueOffset -= (s32)(4.0f * controller->stickAverageX);
                //gameState->greenOffset += (s32)(4.0f * controller->stickAverageY);
                //gameState->toneHz = 256 + (s32)(128.0f * controller->stickAverageX);
            }
            else
            {
                // NOTE(Eran): Delta coordinates are pixels per second and not pixels
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
                
                dPlayerX *= 128.0f;
                dPlayerY *= 128.0f;
                
                f32 newPlayerX =  gameState->playerX + (dPlayerX * input->dTimePerFrame);
                f32 newPlayerY = gameState->playerY + (dPlayerY * input->dTimePerFrame);
                
                RawPosition playerPos = { gameState->playerTileMapX, gameState->playerTileMapY,
                    newPlayerX, newPlayerY };
                
                RawPosition playerLeft = playerPos;
                playerLeft.X -= 0.5f*playerWidth;
                RawPosition playerRight = playerPos;
                playerRight.X += 0.5f*playerWidth;
                
                if (IsWorldPointEmpty(&world,
                                      playerLeft) &&
                    IsWorldPointEmpty(&world,
                                      playerRight) &&
                    IsWorldPointEmpty(&world,
                                      playerPos))
                {
                    CanonicalPosition canPos = GetCanonicalPosition(&world, playerPos);
                    
                    gameState->playerTileMapX = canPos.tileMapX;
                    gameState->playerTileMapY = canPos.tileMapY;
                    
                    gameState->playerX = world.upperLeftX + world.tileWidth * canPos.tileX + canPos.X;
                    gameState->playerY = world.upperLeftY + world.tileHeight * canPos.tileY +canPos.Y;
                }
                
            }
        }
    }
    
    TileMap* tileMap = GetTileMap(&world, gameState->playerTileMapX, gameState->playerTileMapY);
    
    
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
            
            f32 minX = (world.upperLeftX + (f32)(column * world.tileWidth));
            f32 minY = (world.upperLeftY + (f32)(row * world.tileHeight));
            f32 maxX = (f32)(minX + world.tileWidth);
            f32 maxY = (f32)(minY + world.tileHeight);
            
            DrawRectangle(offscreenBuffer, minX, minY, maxX, maxY, tileColor, tileColor, tileColor);
        }
    }
    
    f32 playerR = 1.0f;
    f32 playerG = 0.0f;
    f32 playerB = 1.0f;
    
    
    f32 playerLeft = gameState->playerX - (playerWidth * 0.5f);
    f32 playerTop = gameState->playerY - playerHeight;
    
    DrawRectangle(
        offscreenBuffer,
        playerLeft, playerTop,
        playerLeft + playerWidth, playerTop + playerHeight,
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
