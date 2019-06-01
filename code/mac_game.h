#if !defined(MAC_GAME_H)

struct MacGameCode
{
    void* gameCodeDLL;
    time_t DLLLastWriteTime;
    
    GameUpdateAndRenderType* UpdateAndRender;
    GameGetSoundSamplesType* GetSoundSamples;
    
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

struct MacReplayBuffer
{
    s32 fileHandle;
    char replayFileName[MAC_STATE_FILE_NAME_COUNT];
    void* memoryBlock;
};

struct MacState
{
    u64 totalSize;
    void* gameMemoryBlock;
    
    MacReplayBuffer replayBuffers[4];
    
    s32 recordingHandle;
    s32 inputRecordingIndex;
    
    s32 playBackHandle;
    s32 inputPlayingIndex;
    
    char appFileName[MAC_STATE_FILE_NAME_COUNT];
    char* onePastLastAppFileNameSlash;
};

#define MAC_GAME_H
#endif