#if !defined(WIN32_GAME_H)

struct Win32Backbuffer
{
    BITMAPINFO info;
    void* memory;
    s32 width;
    s32 height;
    s32 pitch;
    s32 bytesPerPixel;
};

struct Win32WindowDimension
{
    s32 width;
    s32 height;
};

struct Win32SoundOutput
{
    s32 samplesPerSecond;
    u32 runningSampleIndex;
    s32 bytesPerSample;
    s32 seconderyBufferSize;
};

#define WIN32_GAME_H
#endif
