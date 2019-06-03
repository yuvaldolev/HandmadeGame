#if !defined(GAME_PLATFORM_H)

#ifdef __cplusplus
extern "C" {
#endif
    
#include "game_types.h"
    
    ////////////////////////////////////
    //          Platform API          //
    ////////////////////////////////////
    typedef struct ThreadContext
    {
        s32 placeHolder;
    } ThreadContext;
    
    // NOTE(yuval): Services that the game provides to the platform
    typedef struct GameSoundOutputBuffer
    {
        s16* samples;
        s32 samplesPerSecond;
        u32 sampleCount;
    } GameSoundOutputBuffer;
    
    typedef struct GameOffscreenBuffer
    {
        void* memory;
        s32 width;
        s32 height;
        s32 pitch;
        s32 bytesPerPixel;
    } GameOffscreenBuffer;
    
    typedef struct GameButtonState
    {
        s32 halfTransitionCount;
        b32 endedDown;
    } GameButtonState;
    
    typedef struct GameController
    {
        b32 isConnected;
        b32 isAnalog;
        
        f32 stickAverageX;
        f32 stickAverageY;
        
        union
        {
            GameButtonState buttons[13];
            
            struct
            {
                GameButtonState moveUp;
                GameButtonState moveDown;
                GameButtonState moveLeft;
                GameButtonState moveRight;
                
                GameButtonState run;
                
                GameButtonState actionUp;
                GameButtonState actionDown;
                GameButtonState actionLeft;
                GameButtonState actionRight;
                
                GameButtonState leftShoulder;
                GameButtonState rightShoulder;
                
                GameButtonState back;
                GameButtonState start;
                
                // IMPORTANT(yuval): Add new buttons before this ButtonState
                GameButtonState terminator;
            };
        };
    } GameController;
    
    typedef struct GameInput
    {
        f32 dtForFrame;
        
        GameButtonState mouseButtons[5];
        s32 mouseX, mouseY, mouseZ;
        
        GameController controllers[5];
    } GameInput;
    
    // NOTE(yuval): Services that the platform provides to the game
#define PLATFORM_DISPLAY_MESSAGE_BOX(name) void name(const char* title, const char* message)
    typedef PLATFORM_DISPLAY_MESSAGE_BOX(PlatformDisplayMessageBoxType);
    
    typedef struct PlatformDateTime
    {
        u16 day;
        u16 month;
        u16 year;
        
        u16 hour;
        u16 minute;
        u16 second;
        u16 milliseconds;
    } PlatformDateTime;
    
#define PLATFORM_GET_DATE_TIME(name) PlatformDateTime name()
    typedef PLATFORM_GET_DATE_TIME(PlatformGetDateTimeType);
    
#ifdef GAME_INTERNAL
    /*
    IMPORTANT(yuval):
    This code is NOT shipping code -
    The functions are blocking and write doesn't protect against lost data!
    */
    typedef struct
    {
        void* contents;
        u32 contentsSize;
    } DEBUGReadFileResult;
    
#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(ThreadContext* thread, void* memory)
    typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemoryType);
    
#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) DEBUGReadFileResult name(ThreadContext* thread, \
    const char* fileName)
    typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFileType);
    
#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) b32 name(ThreadContext* thread, \
    const char* fileName, \
    void* memory, u32 memorySize)
    typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFileType);
#endif
    
    typedef struct PlatformAPI
    {
        PlatformDisplayMessageBoxType* DisplayMessageBox;
        
        PlatformGetDateTimeType* GetDateTime;
        
        DEBUGPlatformFreeFileMemoryType* DEBUGFreeFileMemory;
        DEBUGPlatformReadEntireFileType* DEBUGReadEntireFile;
        DEBUGPlatformWriteEntireFileType* DEBUGWriteEntireFile;
    } PlatformAPI;
    
    extern PlatformAPI platform;
    
#include "game_assert.h"
#include "game_shared.h"
#include "game_intrinsics.h"
    
    typedef struct GameMemory
    {
        b32 isInitialized;
        
        void* permanentStorage; // NOTE(yuval): Memory is REQUIRED to be initialized to 0 at startup
        u64 permanentStorageSize;
        
        void* transientStorage; // NOTE(yuval): Memory is REQUIRED to be initialized to 0 at startup
        u64 transientStorageSize;
        
        PlatformAPI platformAPI;
    } GameMemory;
    
    /* GameUpdateAndRender gets 4 thing from the Platform:
       1. Timing
       2. Controller/Keyboard input
       3. Bitmap buffer to use
       4. Sound buffer to use
    */
#define GAME_UPDATE_AND_RENDER(name) void name(ThreadContext* thread, GameMemory* memory, \
    GameInput* input, \
    GameOffscreenBuffer* offscreenBuffer)
    typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRenderType);
    
#define GAME_GET_SOUND_SAMPLES(name) void name(ThreadContext* thread, GameMemory* memory, \
    GameSoundOutputBuffer* soundBuffer)
    typedef GAME_GET_SOUND_SAMPLES(GameGetSoundSamplesType);
    
#ifdef __cplusplus
}
#endif

#define GAME_PLATFORM_H
#endif