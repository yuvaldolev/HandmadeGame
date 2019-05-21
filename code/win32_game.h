#if !defined(WIN32_GAME_H)

struct Win32GameCode
{
    HMODULE gameCodeDLL;
    FILETIME DLLLastWriteTime;
    
    GameUpdateAndRenderType* UpdateAndRender;
    GameGetSoundSamplesType* GetSoundSamples;
    LogType* Log;
    
    b32 isValid;
};

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
    u32 secondaryBufferSize;
    DWORD safetyBytes;
    // TODO(yuval & eran): Should runningSampleIndex be in bytes?
    // TODO(yuval & eran): Math gets simples if we add a bytesPerSeocond field
};

// TODO(yuval & eran): Move this
#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH

struct Win32ReplayBuffer
{
    HANDLE fileHandle;
    HANDLE memoryMap;
    char replayFileName[WIN32_STATE_FILE_NAME_COUNT];
    void* memoryBlock;
};

struct Win32State
{
    u64 totalSize;
    void* gameMemoryBlock;
    
    Win32ReplayBuffer replayBuffers[4];
    
    HANDLE recordingHandle;
    s32 inputRecordingIndex;
    
    HANDLE playBackHandle;
    s32 inputPlayingIndex;
    
    char exeFileName[WIN32_STATE_FILE_NAME_COUNT];
    char* onePastLastEXEFileNameSlash;
};

struct DEBUGWin32TimeMarker
{
    DWORD outputPlayCursor;
    DWORD outputWriteCursor;
    DWORD outputLocation;
    DWORD outputByteCount;
    DWORD expectedFlipPlayCursor;
    
    
    DWORD flipPlayCursor;
    DWORD flipWriteCursor;
};

#define WIN32_GAME_H
#endif
