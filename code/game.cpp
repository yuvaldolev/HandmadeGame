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
        
        gameState->playerX = 10.0f;
        gameState->playerY = 10.0f;
        
        memory->isInitialized = true;
    }
    
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
                // NOTE (eran): Delta coordinates are pixels per second and not pixels
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
                
                gameState->playerX += (dPlayerX * input->dTimePerFrame);
                gameState->playerY +=  (dPlayerY * input->dTimePerFrame);
            }
        }
    }
    
    u32 tileMap[9][17] = {
        { 0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0,  1, 1, 1, 1, 1 },
        { 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 1, 0 },
        { 0, 0, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0 },
        { 0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0 },
        { 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0 },
        { 0, 0, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 0 },
        { 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0 },
        { 0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0, 0 },
        { 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0 }
    };
    
    u32 tileWidth = 69;
    u32 tileHeight = 69;
    u32 upperLeftX = 0;
    u32 upperLeftY = 0;
    
    for (int row = 0; row < 9; ++row)
    {
        for (int column = 0; column < 17; ++column)
        {
            u32 tileID = tileMap[row][column];
            f32 tileColor = 0.5f;
            
            if (tileID == 1)
            {
                tileColor = 1.0f;
            }
            
            f32 minX = (upperLeftX + (f32)(column * tileWidth));
            f32 minY = (upperLeftY + (f32)(row * tileHeight));
            f32 maxX = (f32)(minX + tileWidth);
            f32 maxY = (f32)(minY + tileHeight);
            
            DrawRectangle(offscreenBuffer, minX, minY, maxX, maxY, tileColor, tileColor, tileColor);
        }
    }
    
    f32 playerR = 1.0f;
    f32 playerG = 0.0f;
    f32 playerB = 1.0f;
    
    f32 playerWidth = tileWidth * 0.75f;
    f32 playerHeight = (f32)tileHeight;
    f32 playerLeft = gameState->playerX - (playerWidth * 0.5f);
    f32 playerTop = gameState->playerY - playerHeight;
    
    DrawRectangle(offscreenBuffer, playerLeft, playerTop,
                  playerWidth + playerLeft, playerHeight + playerTop,
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
