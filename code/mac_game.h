#if !defined(MAC_GAME_H)

struct MacOffscreenBuffer
{
    u8* memory;
    s32 width;
    s32 height;
    s32 pitch;
    s32 bytesPerPixel;
};

#define MAC_GAME_H
#endif