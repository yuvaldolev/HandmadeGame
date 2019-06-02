#include "game.h"

#include "game_log.cpp"
#include "game_tile.cpp"

#include <math.h> // TODO(yuval & eran): Temporary

#define RGB(R, G, B) ((RoundF32ToU32(R * 255.0f) << 16) | \
(RoundF32ToU32(G * 255.0f) << 8) | \
(RoundF32ToU32(B * 255.0f)))

internal LoadedBitmap
DEBUGLoadBMP(ThreadContext* thread,
             DEBUGPlatformReadEntireFileType* DEBUGPlatformReadEntireFile,
             const char* fileName)
{
    LoadedBitmap result = { };
    DEBUGReadFileResult readResult = DEBUGPlatformReadEntireFile(thread, fileName);
    if (readResult.contentsSize != 0)
    {
        BitmapHeader* header = (BitmapHeader*)readResult.contents;
        result.width = header->width;
        result.height = header->height;
        result.pixels = (u32*)((u8*)readResult.contents + header->bitmapOffset);
        
        u32 redMask = header->redMask;
        u32 greenMask = header->greenMask;
        u32 blueMask = header->blueMask;
        u32 alphaMask = ~(header->blueMask | header->redMask | header->greenMask);
        
        
        BitScanResult redShift = FindLeastSignificantSetBit(redMask);
        BitScanResult greenShift = FindLeastSignificantSetBit(greenMask);
        BitScanResult blueShift = FindLeastSignificantSetBit(blueMask);
        BitScanResult alphaShift = FindLeastSignificantSetBit(alphaMask);
        
        
        Assert(redShift.found);
        Assert(greenShift.found);
        Assert(blueShift.found);
        Assert(alphaShift.found);
        
        u32* sourceDest = result.pixels;
        
        for (s32 Y = 0; Y < header->height; ++Y)
        {
            for (s32 X = 0; X < header->width; ++X)
            {
                u32 c = *sourceDest;
                *sourceDest++ = ((((c >> alphaShift.index) & 0xFF) << 24) |
                                 (((c >> redShift.index) & 0xFF) << 16) |
                                 (((c >> greenShift.index) & 0xFF) << 8) |
                                 (((c >> blueShift.index) & 0xFF) << 0));
            }
        }
    }
    return result;
}

internal void
TEMPDrawBitMap(GameOffscreenBuffer* offscreenBuffer, LoadedBitmap* bitmap, f32 realX, f32 realY)
{
    s32 minX = RoundF32ToS32(realX);
    s32 minY = RoundF32ToS32(realY);
    s32 maxX = RoundF32ToS32((f32)bitmap->width + minX);
    s32 maxY = RoundF32ToS32((f32)bitmap->height + minY);
    
    if (minX < 0)
    {
        minX = 0;
    }
    
    if (minY < 0)
    {
        minY = 0;
    }
    
    if (maxX > offscreenBuffer->width)
    {
        maxX= offscreenBuffer->width;
    }
    
    if (maxY> offscreenBuffer->height)
    {
        maxY = offscreenBuffer->height;
    }
    
    u32* sourceRow = bitmap->pixels + bitmap->width *
        (bitmap->height - 1);
    u8* destRow = ((u8*)offscreenBuffer->memory + offscreenBuffer->bytesPerPixel * minX +
                   offscreenBuffer->pitch * minY);
    
    for (s32 Y = minY; Y < maxY; ++Y)
    {
        
        u32* dest = (u32 *)destRow;
        u32* source = sourceRow;
        for (s32 X = minX; X < maxX; ++X)
        {
            f32 alpha = (f32)((*source >> 24) & 0xFF) /255.0f;
            f32 sourceR = (f32)((*source >> 16) & 0xFF);
            f32 sourceG = (f32)((*source >> 8) & 0xFF);
            f32 sourceB = (f32)((*source >> 0) & 0xFF);
            
            f32 destR = (f32)((*dest >> 16) & 0xFF);
            f32 destG = (f32)((*dest >> 8) & 0xFF);
            f32 destB = (f32)((*dest >> 0) & 0xFF);
            
            f32 R = (1.0f - alpha) * destR + alpha * sourceR;
            f32 G = (1.0f - alpha) * destG + alpha * sourceG;
            f32 B = (1.0f - alpha) * destB + alpha * sourceB;
            
            *dest = (((u32)(R + 0.5f) << 16) |
                     ((u32)(G + 0.5f) << 8) |
                     ((u32)(B + 0.5f) << 0));
            
            
            dest++;
            source++;
        }
        
        destRow += offscreenBuffer->pitch;
        sourceRow -= bitmap->width;
    }
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
    
    // TODO(yuval & eran): @Temporary
    globalLogMemory = memory;
    globalLogFormatMemory = memory;
    
    if (!memory->isInitialized)
    {
        InitializeArena(&gameState->loggingArena,
                        (u8*)memory->permanentStorage + sizeof(GameState),
                        (u64)(memory->permanentStorageSize * 0.0001f));
        
        LogInit(&gameState->loggingArena, LogLevelDebug, "[%V] [%d] %f:%U:%L: %m%n");
        
        LogDebug("Random Number Amount: %d\n", ArrayCount(randomNumberTable));
        
        gameState->playerP.absTileX = 1;
        gameState->playerP.absTileY = 3;
        gameState->playerP.tileRelX = 5.0f;
        gameState->playerP.tileRelY = 5.0f;
        
        InitializeArena(&gameState->worldArena,
                        gameState->loggingArena.base + gameState->loggingArena.size,
                        memory->permanentStorageSize - sizeof(GameState) -
                        gameState->loggingArena.size);
        
        gameState->world = PushStruct(&gameState->worldArena, World);
        World* world = gameState->world;
        world->tileMap = PushStruct(&gameState->worldArena, TileMap);
        
        TileMap* tileMap = world->tileMap;
        
        // NOTE: This is set to be using 256x256 tile chunks
        tileMap->chunkShift = 4;
        tileMap->chunkMask = (1 << tileMap->chunkShift) - 1;
        tileMap->chunkDim = (1 << tileMap->chunkShift);
        
        tileMap->tileChunkCountX = 128;
        tileMap->tileChunkCountY = 128;
        tileMap->tileChunkCountZ = 2;
        tileMap->tileChunks = PushArray(&gameState->worldArena, TileChunk,
                                        tileMap->tileChunkCountX * tileMap->tileChunkCountY *
                                        tileMap->tileChunkCountZ);
        
        tileMap->tileSideInMeters = 1.4f;
        
        u32 randomNumberIndex = 0;
        
        const u32 TILES_PER_WIDTH = 17;
        const u32 TILES_PER_HEIGHT = 9;
        
        u32 screenX = 0;
        u32 screenY = 0;
        u32 absTileZ = 0;
        
        b32 doorLeft = false;
        b32 doorRight = false;
        b32 doorTop = false;
        b32 doorBottom = false;
        b32 doorUp = false;
        b32 doorDown = false;
        
        for (u32 screenIndex = 0; screenIndex < 100; ++screenIndex)
        {
            // TODO(yuval, eran): Random number generator
            Assert(randomNumberIndex < ArrayCount(randomNumberTable));
            u32 randomChoice;
            
            if (doorUp || doorDown)
            {
                randomChoice = randomNumberTable[randomNumberIndex++] % 2;
            }
            else
            {
                randomChoice = randomNumberTable[randomNumberIndex++] % 3;
            }
            
            b32 createdZDoor = false;
            
            if (randomChoice == 2)
            {
                createdZDoor = true;
                
                if (absTileZ == 0)
                {
                    doorUp = true;
                }
                else
                {
                    doorDown = true;
                }
            }
            else if (randomChoice == 1)
            {
                doorRight = true;
            }
            else
            {
                doorTop = true;
            }
            
            for (u32 tileY = 0; tileY < TILES_PER_HEIGHT; ++tileY)
            {
                for (u32 tileX = 0; tileX < TILES_PER_WIDTH; ++tileX)
                {
                    u32 absTileX = screenX * TILES_PER_WIDTH + tileX;
                    u32 absTileY = screenY * TILES_PER_HEIGHT + tileY;
                    
                    u32 tileValue = 1;
                    
                    if (tileX == 0 && (!doorLeft || (tileY != (TILES_PER_HEIGHT / 2))))
                    {
                        tileValue = 2;
                    }
                    
                    if ((tileX == (TILES_PER_WIDTH - 1)) &&
                        (!doorRight || (tileY != (TILES_PER_HEIGHT / 2))))
                    {
                        tileValue = 2;
                    }
                    
                    if (tileY == 0 && (!doorBottom || (tileX != (TILES_PER_WIDTH / 2))))
                    {
                        tileValue = 2;
                    }
                    
                    if ((tileY == TILES_PER_HEIGHT - 1) &&
                        (!doorTop || (tileX != (TILES_PER_WIDTH / 2))))
                    {
                        tileValue = 2;
                    }
                    
                    if ((tileX == 10) && (tileY == 6))
                    {
                        if (doorUp)
                        {
                            tileValue = 3;
                        }
                        else if (doorDown)
                        {
                            tileValue = 4;
                        }
                    }
                    
                    SetTileValue(&gameState->worldArena, world->tileMap,
                                 absTileX, absTileY, absTileZ, tileValue);
                }
            }
            
            doorLeft = doorRight;
            doorBottom = doorTop;
            
            doorRight = false;
            doorTop = false;
            
            if (createdZDoor)
            {
                doorUp = !doorUp;
                doorDown = !doorDown;
            }
            else
            {
                doorUp = false;
                doorDown = false;
            }
            
            if (randomChoice == 2)
            {
                if (absTileZ == 0)
                {
                    absTileZ = 1;
                }
                else
                {
                    absTileZ = 0;
                }
            }
            else if (randomChoice == 1)
            {
                ++screenX;
            }
            else
            {
                ++screenY;
            }
        }
        
        gameState->backdrop = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "../data/test/test_background.bmp");
        
        gameState->heroBitmap[0].head = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "../data/test/test_hero_right_head.bmp");
        gameState->heroBitmap[0].cape = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "../data/test/test_hero_right_cape.bmp");
        gameState->heroBitmap[0].torso = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "../data/test/test_hero_right_torso.bmp");
        
        gameState->heroBitmap[1].head = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "../data/test/test_hero_back_head.bmp");
        gameState->heroBitmap[1].cape = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "../data/test/test_hero_back_cape.bmp");
        gameState->heroBitmap[1].torso = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "../data/test/test_hero_back_torso.bmp");
        
        gameState->heroBitmap[2].head = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "../data/test/test_hero_left_head.bmp");
        gameState->heroBitmap[2].cape = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "../data/test/test_hero_left_cape.bmp");
        gameState->heroBitmap[2].torso = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "../data/test/test_hero_left_torso.bmp");
        
        gameState->heroBitmap[3].head = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "../data/test/test_hero_front_head.bmp");
        gameState->heroBitmap[3].cape = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "../data/test/test_hero_front_cape.bmp");
        gameState->heroBitmap[3].torso = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "../data/test/test_hero_front_torso.bmp");
        
        gameState->facingDirection = 3;
        
        memory->isInitialized = true;
    }
    
    f32 playerHeight = 1.4f;
    f32 playerWidth = playerHeight * 0.75f;
    
    World* world = gameState->world;
    TileMap* tileMap = world->tileMap;
    
    s32 tileSideInPixels = 60;
    f32 metersToPixels = (f32)tileSideInPixels / tileMap->tileSideInMeters;
    
    f32 lowerLeftX = -(f32)tileSideInPixels / 2.0f;
    f32 lowerLeftY = (f32)offscreenBuffer->height;
    
    for (u32 controllerIndex = 0;
         controllerIndex < ArrayCount(input->controllers);
         ++controllerIndex)
    {
        GameController* controller = &input->controllers[controllerIndex];
        
        if (controller->isConnected)
        {
            // NOTE: Delta coordinates are meters per second
            f32 dPlayerX = 0.0f;
            f32 dPlayerY = 0.0f;
            
            if (controller->isAnalog)
            {
                // TODO(yuval, eran): Analog controller tuning
                dPlayerX = controller->stickAverageX;
                dPlayerY = controller->stickAverageY;
            }
            else
            {
                // TODO(yuval, eran): Digital controller tuning
                if (controller->moveUp.endedDown)
                {
                    dPlayerY = 1.0f;
                    gameState->facingDirection = 1;
                }
                
                if (controller->moveDown.endedDown)
                {
                    dPlayerY = -1.0f;
                    gameState->facingDirection = 3;
                }
                
                if (controller->moveLeft.endedDown)
                {
                    dPlayerX = -1.0f;
                    gameState->facingDirection = 2;
                }
                
                if (controller->moveRight.endedDown)
                {
                    dPlayerX = 1.0f;
                    gameState->facingDirection = 0;
                }
            }
            
            f32 playerSpeed = 2.0f;
            
            if (controller->run.endedDown)
            {
                playerSpeed = 10.f;
            }
            
            dPlayerX *= playerSpeed;
            dPlayerY *= playerSpeed;
            
            TileMapPosition newPlayerP = gameState->playerP;
            newPlayerP.tileRelX += dPlayerX * input->dtForFrame;
            newPlayerP.tileRelY += dPlayerY * input->dtForFrame;
            newPlayerP = RecanonicalizePosition(tileMap, newPlayerP);
            
            TileMapPosition playerLeft = newPlayerP;
            playerLeft.tileRelX -= 0.5f * playerWidth;
            playerLeft = RecanonicalizePosition(tileMap, playerLeft);
            
            TileMapPosition playerRight = newPlayerP;
            playerRight.tileRelX += 0.5f * playerWidth;
            playerRight = RecanonicalizePosition(tileMap, playerRight);
            
            if (IsTileMapPointEmpty(tileMap, newPlayerP) &&
                IsTileMapPointEmpty(tileMap, playerLeft) &&
                IsTileMapPointEmpty(tileMap, playerRight))
            {
                if (!AreOnSameTile(&gameState->playerP, &newPlayerP))
                {
                    u32 newTileValue = GetTileValue(tileMap, newPlayerP);
                    
                    if (newTileValue == 3)
                    {
                        ++newPlayerP.absTileZ;
                    }
                    else if (newTileValue == 4)
                    {
                        -- newPlayerP.absTileZ;
                    }
                }
                
                gameState->playerP = newPlayerP;
            }
        }
    }
    
    TEMPDrawBitMap(offscreenBuffer, &gameState->backdrop, 0, 0);
    
    f32 screenCenterX = (f32)offscreenBuffer->width / 2.0f;
    f32 screenCenterY  = (f32)offscreenBuffer->height / 2.0f;
    
    for (s32 relRow = -10; relRow < 10; ++relRow)
    {
        for (s32 relColumn = -20; relColumn < 20; ++relColumn)
        {
            u32 column = gameState->playerP.absTileX + relColumn;
            u32 row =  gameState->playerP.absTileY + relRow;
            u32 tileID = GetTileValue(tileMap, column, row, gameState->playerP.absTileZ);
            
            if (tileID > 0)
            {
                f32 tileColor = 0.5f;
                
                if (tileID == 2)
                {
                    tileColor = 1.0f;
                }
                
                if (tileID > 2)
                {
                    tileColor = 0.25f;
                }
                
                if ((column == gameState->playerP.absTileX) &&
                    (row == gameState->playerP.absTileY))
                {
                    tileColor = 0.0f;
                }
                
                f32 cenX = screenCenterX - gameState->playerP.tileRelX * metersToPixels +
                    (f32)(relColumn * tileSideInPixels);
                f32 cenY = screenCenterY + gameState->playerP.tileRelY * metersToPixels -
                    (f32)(relRow * tileSideInPixels);
                
                f32 minX = cenX - 0.5f * tileSideInPixels;
                f32 minY = cenY - 0.5f * tileSideInPixels;
                f32 maxX = cenX + 0.5f * tileSideInPixels;
                f32 maxY = cenY + 0.5f * tileSideInPixels;
                
                DrawRectangle(offscreenBuffer,
                              minX, minY, maxX, maxY,
                              tileColor, tileColor, tileColor);
            }
        }
    }
    
    f32 playerR = 1.0f;
    f32 playerG = 1.0f;
    f32 playerB = 0.0f;
    
    f32 playerLeft = screenCenterX - 0.5f * playerWidth * metersToPixels;
    f32 playerTop = screenCenterY - playerHeight * metersToPixels;
    
    DrawRectangle(offscreenBuffer,
                  playerLeft, playerTop,
                  playerLeft + playerWidth * metersToPixels,
                  playerTop + playerHeight * metersToPixels,
                  playerR, playerG, playerB);
    
    /*HeroBitmap* heroBitmap = &gameState->heroBitmap[gameState->facingDirection];
    
    TEMPDrawBitMap(offscreenBuffer, &heroBitmap->head, playerLeft, playerTop);
    TEMPDrawBitMap(offscreenBuffer, &heroBitmap->cape, playerLeft, playerTop);
    TEMPDrawBitMap(offscreenBuffer, &heroBitmap->torso, playerLeft, playerTop);*/
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    GameState* gameState = (GameState*)memory->permanentStorage;
    GameOutputSound(gameState, soundBuffer, 1);
}
