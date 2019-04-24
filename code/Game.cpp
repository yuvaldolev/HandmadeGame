#include "game.h"

#include "game_log.cpp"

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

void
GameUpdateAndRender(GameOffscreenBuffer* buffer)
{
    int blueOffset = 0;
    int greenOffset = 0;
    RenderGradient(buffer, blueOffset, greenOffset);
}

