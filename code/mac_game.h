#if !defined(MAC_GAME_H)

struct mac_game_code
{
    void* GameCodeDLL;
    time_t DLLLastWriteTime;
    
    // IMPORTANT: Either of the callbacks can be 0!  You must
    // check before calling.
    game_update_and_render* UpdateAndRender;
    game_get_sound_samples* GetSoundSamples;
    
    b32 IsValid;
};

struct mac_offscreen_buffer
{
    u32 TextureID;
    void* Memory;
    s32 Width;
    s32 Height;
    s32 Pitch;
    s32 BytesPerPixel;
};

struct mac_sound_output
{
    game_sound_output_buffer SoundBuffer;
    u32 SoundBufferSize;
    
    s16* CoreAudioBuffer;
    s16* ReadCursor;
    s16* WriteCursor;
};

#define MAC_STATE_FILE_NAME_COUNT FILENAME_MAX

struct mac_replay_buffer
{
    s32 FileHandle;
    char ReplayFileName[MAC_STATE_FILE_NAME_COUNT];
    void* MemoryBlock;
};

struct mac_state
{
    u64 TotalSize;
    void* GameMemoryBlock;
    
    mac_replay_buffer ReplayBuffers[4];
    
    s32 RecordingHandle;
    s32 InputRecordingIndex;
    
    s32 PlayBackHandle;
    s32 InputPlayingIndex;
    
    char AppFileName[MAC_STATE_FILE_NAME_COUNT];
    char* OnePastLastAppFileNameSlash;
};

#define MAC_GAME_H
#endif