#include "game.h"

#include <SDL2/SDL.h>
#include <sys/mman.h>
#include <sys/syslimits.h>

#include "sdl_game.h"

global_variable b32 globalRunning;
global_variable SDLOffscreenBuffer globalBackbuffer;
global_variable u64 globalPerfCountFrequency;

internal void
RenderWeirdGradient(SDLOffscreenBuffer* Buffer, int BlueOffset, int GreenOffset)
{
    u8 *Row = (u8*)Buffer->memory;
    for(int Y = 0;
        Y < Buffer->height;
        ++Y)
    {
        u32 *Pixel = (u32*)Row;
        for(int X = 0;
            X < Buffer->width;
            ++X)
        {
            u8 Blue = (X + BlueOffset);
            u8 Green = (Y + GreenOffset);

            *Pixel++ = ((Green << 8) | Blue);
        }

        Row += Buffer->pitch;
    }
}

internal void
SDLUpdateWindow(SDL_Window* window, SDL_Renderer* renderer, SDLOffscreenBuffer* buffer)
{
    SDL_UpdateTexture(buffer->texture,
                      0,
                      buffer->memory,
                      buffer->pitch);

    SDL_RenderCopy(renderer,
                   buffer->texture,
                   0, 0);

    SDL_RenderPresent(renderer);
}

internal SDLWindowDimension
SDLGetWindowDimension(SDL_Window* window)
{
    SDLWindowDimension result;
    SDL_GetWindowSize(window, &result.width, &result.height);
    return result;
}

internal void
SDLResizeTexture(SDLOffscreenBuffer* buffer, SDL_Renderer* renderer,
                 s32 width, s32 height)
{
    const s32 BYTES_PER_PIXEL = 4;

    if (buffer->memory)
    {
        munmap(buffer->memory,
               buffer->width * buffer->height * buffer->bytesPerPixel);
    }

    if (buffer->texture)
    {
        SDL_DestroyTexture(buffer->texture);
    }

    buffer->texture = SDL_CreateTexture(renderer,
                                        SDL_PIXELFORMAT_ARGB8888,
                                        SDL_TEXTUREACCESS_STREAMING,
                                        width, height);

    buffer->width = width;
    buffer->height = height;
    buffer->pitch = width * BYTES_PER_PIXEL;
    buffer->bytesPerPixel = BYTES_PER_PIXEL;
    buffer->memory = mmap(0,
                          width * height * BYTES_PER_PIXEL,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1, 0);
}

internal void
HandleEvent(SDL_Event* event)
{
    switch (event->type)
    {
        case SDL_QUIT:
        {
            globalRunning = false;
        } break;

        case SDL_WINDOWEVENT:
        {
            switch (event->window.event)
            {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                {

                } break;

                case SDL_WINDOWEVENT_FOCUS_GAINED:
                {

                } break;

                case SDL_WINDOWEVENT_EXPOSED:
                {
                    SDL_Window *window = SDL_GetWindowFromID(event->window.windowID);
                    SDL_Renderer *renderer = SDL_GetRenderer(window);
                    SDLUpdateWindow(window, renderer, &globalBackbuffer);
                } break;
            }
        } break;
    }
}

void
CatStrings(char* dest, memory_index destCount,
           const char* sourceA, memory_index sourceACount,
           const char* sourceB, memory_index sourceBCount)
{
    Assert(destCount > sourceACount + sourceBCount);
    char* destAt = dest;

    // TODO(yuval & eran): @Copy-and-paste
    // TODO(yuval & eran): @Incomplete use String struct
    for (s32 index = 0; index < sourceACount; ++index)
    {
        *destAt++ = sourceA[index];
    }

    for (s32 index = 0; index < sourceBCount; ++index)
    {
        *destAt++ = sourceB[index];
    }

    *destAt = 0;
}

int main(int argc, char* argv[])
{
    char* exeFilePath = SDL_GetBasePath();
    char* onePastSlash = exeFilePath + StringLength(exeFilePath);

    for (char* scan = exeFilePath; *scan; ++scan)
    {
        if (*scan == '/')
        {
            onePastSlash = scan + 1;
        }
    }

    // TODO(yuval): @Check-out sizeof
    const char* sourceGameCodeSOFileName = "game.so";
    char sourceGameCodeSOFullPath[PATH_MAX];
    CatStrings(sourceGameCodeSOFullPath, sizeof(sourceGameCodeSOFullPath),
               exeFilePath, onePastSlash - exeFilePath,
               sourceGameCodeSOFileName, StringLength(sourceGameCodeSOFileName));


    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC | SDL_INIT_AUDIO);
    globalPerfCountFrequency = SDL_GetPerformanceFrequency();

    SDL_Window* window = SDL_CreateWindow("Handmade Game",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          1280,
                                          720,
                                          SDL_WINDOW_RESIZABLE);

    if (window)
    {
        SDL_Renderer* renderer = SDL_CreateRenderer(window,
                                                    -1,
                                                    SDL_RENDERER_PRESENTVSYNC);

        if (renderer)
        {
            SDLWindowDimension dimension = SDLGetWindowDimension(window);
            SDLResizeTexture(&globalBackbuffer, renderer, dimension.width, dimension.height);

            s32 xOffset = 0;
            s32 yOffset = 0;

            globalRunning = true;

            while (globalRunning)
            {
                SDL_Event event;
                while (SDL_PollEvent(&event))
                {
                    HandleEvent(&event);
                }

                RenderWeirdGradient(&globalBackbuffer, xOffset++, yOffset++);
                SDLUpdateWindow(window, renderer, &globalBackbuffer);
            }
        }
    }
}

