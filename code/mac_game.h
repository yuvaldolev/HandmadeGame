#if !defined(MAC_GAME_H)

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

#define MAC_GAME_H
#endif