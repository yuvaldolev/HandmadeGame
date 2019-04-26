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
GameOutputSound(GameSoundOutputBuffer* buffer)
{
    local_persist r32 tSine;

    const s32 TONE_HZ = 256;
    const s32 TONE_VOLUME = 16000;
    const s32 WAVE_PERIOD = buffer->samplesPerSecond / TONE_HZ;

    s16* sampleOut = (s16*)buffer->samples;

    for (s32 sampleIndex = 0;
         sampleIndex < buffer->sampleCount;
         ++sampleIndex)
    {
        r32 sineValue = sinf(tSine);
        s16 sampleValue = (s16)(sineValue * TONE_VOLUME);
        *sampleOut++ = sampleValue;
        *sampleOut++ = sampleValue;

        tSine -= 2.0f * Pi32 * (1.0f / (r32)WAVE_PERIOD);
    }

}

void
GameUpdateAndRender(GameOffscreenBuffer* offscreenBuffer,
                    GameSoundOutputBuffer* soundBuffer)
{
    int blueOffset = 0;
    int greenOffset = 0;

    if (input.isAnalog)
    {
        // TODO(yuval & eran): Analog controller tuning
    }
    else
    {
        // TODO(yuval & eran): Digital controller tuning
    }

    GameOutputSound(soundBuffer);
    RenderGradient(offscreenBuffer, blueOffset, greenOffset);
}

