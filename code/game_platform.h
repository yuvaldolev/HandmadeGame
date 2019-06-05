#if !defined(GAME_PLATFORM_H)

#ifdef __cplusplus
extern "C" {
#endif
    
#include "game_types.h"
    
    ////////////////////////////////////
    //          Platform API          //
    ////////////////////////////////////
    typedef struct thread_context
    {
        s32 PlaceHolder;
    } thread_context;
    
    
    // NOTE(yuval): Services that the platform provides to the game
#define PLATFORM_DISPLAY_MESSAGE_BOX(name) void name(const char* title, const char* message)
    typedef PLATFORM_DISPLAY_MESSAGE_BOX(platform_display_message_box);
    
    typedef struct platform_date_time
    {
        u16 Day;
        u16 Month;
        u16 Year;
        
        u16 Hour;
        u16 Minute;
        u16 Second;
        u16 Milliseconds;
    } platform_date_time;
    
#define PLATFORM_GET_DATE_TIME(name) platform_date_time name()
    typedef PLATFORM_GET_DATE_TIME(platform_get_date_time);
    
#ifdef GAME_INTERNAL
    /*
    IMPORTANT(yuval):
    This code is NOT shipping code -
    The functions are blocking and write doesn't protect against lost data!
    */
    typedef struct debug_read_file_result
    {
        void* Contents;
        u32 ContentsSize;
    } debug_read_file_result;
    
#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(thread_context* Thread, void* Memory)
    typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);
    
#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(thread_context* Thread, \
    const char* FileName)
    typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);
    
#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) b32 name(thread_context* Thread, \
    const char* FileName, \
    void* Memory, u32 MemorySize)
    typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);
#endif
    
    typedef struct platform_api
    {
        platform_display_message_box* DisplayMessageBox;
        
        platform_get_date_time* GetDateTime;
        
        debug_platform_free_file_memory* DEBUGFreeFileMemory;
        debug_platform_read_entire_file* DEBUGReadEntireFile;
        debug_platform_write_entire_file* DEBUGWriteEntireFile;
    } platform_api;
    
    extern platform_api Platform;
    
#include "game_assert.h"
#include "game_shared.h"
#include "game_intrinsics.h"
    
    // NOTE(yuval): Services that the game provides to the platform
    typedef struct game_offscreen_buffer
    {
        void* memory;
        s32 width;
        s32 height;
        s32 pitch;
        s32 bytesPerPixel;
    } game_offscreen_buffer;
    
    typedef struct game_sound_output_buffer
    {
        s16* samples;
        s32 samplesPerSecond;
        u32 sampleCount;
    } game_sound_output_buffer;
    
    typedef struct game_button_state
    {
        s32 halfTransitionCount;
        b32 endedDown;
    } game_button_state;
    
    typedef struct game_controller
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
    } game_controller;
    
    typedef struct game_input
    {
        f32 dtForFrame;
        
        GameButtonState mouseButtons[5];
        s32 mouseX, mouseY, mouseZ;
        
        GameController controllers[5];
    } game_input;
    
    typedef struct game_memory
    {
        b32 isInitialized;
        
        void* permanentStorage; // NOTE(yuval): Memory is REQUIRED to be initialized to 0 at startup
        u64 permanentStorageSize;
        
        void* transientStorage; // NOTE(yuval): Memory is REQUIRED to be initialized to 0 at startup
        u64 transientStorageSize;
        
        PlatformAPI platformAPI;
    } game_memory;
    
    /* GameUpdateAndRender gets 4 thing from the Platform:
       1. Timing
       2. Controller/Keyboard input
       3. Bitmap buffer to use
       4. Sound buffer to use
    */
#define GAME_UPDATE_AND_RENDER(name) void name(thread_context* Thread, game_memory* Memory, \
    game_input* Input, \
    game_offscreen_buffer* OffscreenBuffer)
    typedef GAME_UPDATE_AND_RENDER(game_update_and_render);
    
#define GAME_GET_SOUND_SAMPLES(name) void name(thread_context* Thread, game_memory* Memory, \
    game_sound_output_buffer* SoundBuffer)
    typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);
    
#ifdef __cplusplus
}
#endif

#define GAME_PLATFORM_H
#endif