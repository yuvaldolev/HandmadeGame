#include "game.h"

#include "game_log.cpp"

#include <math.h> // TODO(yuval & eran): Temporary

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
            
            *pixel++ = (Green << 8) | Blue;
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
        f32 sineValue = sinf(gameState->tSine);
        s16 sampleValue = (s16)(sineValue * TONE_VOLUME);
        *sampleOut++ = sampleValue;
        *sampleOut++ = sampleValue;
        
        gameState->tSine += 2.0f * Pi32 * (1.0f / (f32)WAVE_PERIOD);
        
        if (gameState->tSine > 2.0f * Pi32)
        {
            gameState->tSine -= 2.0f * Pi32;
        }
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
        
        gameState->blueOffset = 0;
        gameState->greenOffset = 0;
        
        gameState->tSine = 0.0f;
        
        memory->isInitialized = true;
    }
    
    gameState->toneHz = 256;
    
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
                gameState->blueOffset -= (s32)(4.0f * controller->stickAverageX);
                gameState->greenOffset += (s32)(4.0f * controller->stickAverageY);
                gameState->toneHz = 256 + (s32)(128.0f * controller->stickAverageX);
            }
            else
            {
                // TODO(yuval & eran): Digital controller tuning
                if (controller->moveUp.endedDown)
                {
                    gameState->greenOffset += 1;
                }
                
                if (controller->moveDown.endedDown)
                {
                    gameState->greenOffset -= 1;
                }
                
                if (controller->moveRight.endedDown)
                {
                    gameState->blueOffset -= 1;
                    gameState->toneHz = 256 + 64;
                }
                
                if (controller->moveLeft.endedDown)
                {
                    gameState->blueOffset += 1;
                    gameState->toneHz = 256 - 64;
                }
            }
        }
    }
    
    RenderGradient(offscreenBuffer, gameState->blueOffset,
                   gameState->greenOffset);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    GameState* gameState = (GameState*)memory->permanentStorage;
    GameOutputSound(gameState, soundBuffer, gameState->toneHz);
}