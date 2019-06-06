#if !defined(WIN32_GAME_H)

struct win32_game_code
{
    HMODULE GameCodeDLL;
    FILETIME DLLLastWriteTime;
    
    // IMPORTANT: Either of the callbacks can be 0!  You must
    // check before calling.
    GameUpdateAndRenderType* UpdateAndRender;
    GameGetSoundSamplesType* GetSoundSamples;
    
    b32 IsValid;
};

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void* Memory;
    s32 Width;
    s32 Height;
    s32 Pitch;
    s32 BytesPerPixel;
};

struct win32_window_dimension
{
    s32 Width;
    s32 Height;
};

struct win32_sound_output
{
    s32 SamplesPerSecond;
    u32 RunningSampleIndex;
    s32 BytesPerSample;
    u32 SecondaryBufferSize;
    DWORD SafetyBytes;
    // TODO(yuval & eran): Should runningSampleIndex be in bytes?
    // TODO(yuval & eran): Math gets simples if we add a bytesPerSeocond field
};

// TODO(yuval & eran): Move this
#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH

struct win32_replay_buffer
{
    HANDLE FileHandle;
    HANDLE MemoryMap;
    char ReplayFileName[WIN32_STATE_FILE_NAME_COUNT];
    void* MemoryBlock;
};

struct win32_state
{
    u64 TotalSize;
    void* GameMemoryBlock;
    
    win32_replay_buffer ReplayBuffers[4];
    
    HANDLE RecordingHandle;
    s32 InputRecordingIndex;
    
    HANDLE PlayBackHandle;
    s32 InputPlayingIndex;
    
    char EXEFileName[WIN32_STATE_FILE_NAME_COUNT];
    char* OnePastLastEXEFileNameSlash;
};

struct win32_debug_time_marker
{
    DWORD OutputPlayCursor;
    DWORD OutputWriteCursor;
    DWORD OutputLocation;
    DWORD OutputByteCount;
    DWORD ExpectedFlipPlayCursor;
    
    DWORD FlipPlayCursor;
    DWORD FlipWriteCursor;
};

#define WIN32_GAME_H
#endif
