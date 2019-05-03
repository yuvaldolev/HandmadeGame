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
GameOutputSound(GameSoundOutputBuffer* buffer, const s32 toneHz)
{
    local_persist r32 tSine;
    
    const s32 TONE_VOLUME = 16000;
    const s32 WAVE_PERIOD = buffer->samplesPerSecond / toneHz;
    
    s16* sampleOut = (s16*)buffer->samples;
    
    for (s32 sampleIndex = 0;
         sampleIndex < buffer->sampleCount;
         ++sampleIndex)
    {
        r32 sineValue = sinf(tSine);
        s16 sampleValue = (s16)(sineValue * TONE_VOLUME);
        *sampleOut++ = sampleValue;
        *sampleOut++ = sampleValue;
        
        tSine -= 2.0f * (r32)Pi32 * (1.0f / (r32)WAVE_PERIOD);
    }
}

void
GameUpdateAndRender(GameMemory* memory, GameInput* input,
                    GameOffscreenBuffer* offscreenBuffer,
                    GameSoundOutputBuffer* soundBuffer)
{
    Assert(sizeof(GameMemory) <= memory->permanentStorageSize);
    
    GameState* gameState = (GameState*)memory->permanentStorage;
    
    if (!memory->isInitialized)
    {
        InitializeArena(&memory->loggingArena,
                        (u8*)memory->permanentStorage + sizeof(GameState),
                        (u64)(memory->permanentStorageSize * 0.001f));
        
        LogInit(&memory->loggingArena, LogLevelDebug, "[%V] [%d] %f:%U:%L - %m%n");
        
        gameState->toneHz = 256;
        gameState->blueOffset = 0;
        gameState->greenOffset = 0;
        
        memory->isInitialized = true;
    }
    
    GameController* input0 = &input->controllers[0];
    
    if (input0->isAnalog)
    {
        // TODO(yuval & eran): Analog controller tuning
        gameState->blueOffset -= (s32)(4.0f * input0->endX);
        gameState->greenOffset += (s32)(4.0f * input0->endY);
        gameState->toneHz = 256 + (s32)(128.0f * input0->endY);
    }
    else
    {
        // TODO(yuval & eran): Digital controller tuning
    }
    
    if (input0->down.endedDown)
    {
        gameState->greenOffset += 1;
    }
    
    GameOutputSound(soundBuffer, gameState->toneHz);
    RenderGradient(offscreenBuffer, gameState->blueOffset,
                   gameState->greenOffset);
}
