#if !defined(MAC_GAME_H)

struct MacGameCode
{
    void* gameCodeDLL;
    time_t DLLLastWriteTime;
    
    GameUpdateAndRenderType* UpdateAndRender;
    GameGetSoundSamplesType* GetSoundSamples;
    LogType* Log;
    
    b32 isValid;
};

struct MacOffscreenBuffer
{
    u32 textureID;
    void* memory;
    s32 width;
    s32 height;
    s32 pitch;
    s32 bytesPerPixel;
};

struct MacSoundOutput
{
    GameSoundOutputBuffer soundBuffer;
    u32 soundBufferSize;
    
    s16* coreAudioBuffer;
    s16* readCursor;
    s16* writeCursor;
};

#define MAC_STATE_FILE_NAME_COUNT FILENAME_MAX
struct MacState
{
    u64 totalSize;
    void* gameMemoryBlock;
    
    char appFileName[MAC_STATE_FILE_NAME_COUNT];
    char* onePastLastAppFileNameSlash;
};

#define MAC_GAME_H
#endif