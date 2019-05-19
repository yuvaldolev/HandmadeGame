#if !defined(SDL_GAME_H)

struct SDLOffscreenBuffer
{
    SDL_Texture* texture;
    void* memory;
    s32 width;
    s32 height;
    s32 pitch;
    s32 bytesPerPixel;
};

struct SDLWindowDimension
{
    s32 width;
    s32 height;
};

#define SDL_GAME_H
#endif

