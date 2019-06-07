#include "game_platform.h"
#include "game_shared.h"

#include <windows.h>
#include <dsound.h>
#include <xinput.h>

#include <math.h>
#include <stdio.h> // TODO(yuvai & eran): Remove this temporary include

#include "win32_game.h"

platform_api Platform;

/*
  TODO(yuval & eran): What is left to be done in the platform layer:
  * Saved game location
  * Get a handle to our own executable file
  * Asset loading
  * Threading (launching a thread or multiple threads)
  * Raw Input (support multiple keyboards in particular awsd keys)
  * Sleep / timeBeginPeriod
  * ClipCursor() (For multi monitor support)
  * Fullscreen support
  * WM_SETCURSOR (control cursor sensitivity)
  * QueryCancelAutoplay
  * WM_ACTIVATEAPP (for when we are not the active application)
  * Blit speed improvements (BitBlt)
  * Hardware Accelaration (OpenGL or Direct3D [DirectX] or Both)
  * GetKeyboardLayout (for international keyboards)
  
  This is a partial list
 */

// TODO(yuval & eran): This is temporary!!! Remove this as soon as possible
#define X_INPUT_GET_STATE(Name) DWORD WINAPI Name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);

X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}

global_variable x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(Name) DWORD WINAPI Name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);

X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}

global_variable x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(Name) HRESULT WINAPI Name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

global_variable b32 GlobalRunning;
global_variable b32 GlobalPause;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable IDirectSoundBuffer* GlobalSecondaryBuffer;
global_variable s64 GlobalPerfCountFrequency;
global_variable b32 GlobalShowCursor;
global_variable WINDOWPLACEMENT GlobalWindowPosition;

PLATFORM_DISPLAY_MESSAGE_BOX(PlatformDisplayMessageBox)
{
    MessageBoxA(0, Message, Title, MB_OK | MB_ICONERROR);
}

PLATFORM_GET_DATE_TIME(PlatformGetDateTime)
{
    SYSTEMTIME LocalTime;
    GetLocalTime(&LocalTime);
    
    platform_date_time Result;
    Result.Day = LocalTime.wDay;
    Result.Month = LocalTime.wMonth;
    Result.Year = LocalTime.wYear;
    Result.Hour = LocalTime.wHour;
    Result.Minute = LocalTime.wMinute;
    Result.Second = LocalTime.wSecond;
    Result.Milliseconds = LocalTime.wMilliseconds;
    
    return Result;
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
    if (Memory)
    {
        VirtualFree(Memory, 0, MEM_RELEASE);
    }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
    debug_read_file_result Result = { };
    
    HANDLE FileHandle = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ,
                                    0, OPEN_EXISTING, 0, 0);
    
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize;
        if (GetFileSizeEx(FileHandle, &FileSize))
        {
            u32 FileSize32 = SafeTruncateToU32(FileSize.QuadPart);
            Result.Contents = VirtualAlloc(0, FileSize32,
                                           MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            
            if (Result.Contents)
            {
                DWORD BytesRead;
                
                if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) &&
                    (BytesRead == FileSize32))
                {
                    // NOTE(yuval): The file was read successfully
                    Result.ContentsSize = FileSize32;
                }
                else
                {
                    // Win32LogError("Reading File: %s Failed!", FileName);
                    DEBUGPlatformFreeFileMemory(Thread, Result.Contents);
                    Result.Contents = 0;
                }
            }
            else
            {
                // Win32LogError("Allocation Faliure While Read File: %s", FileName);
            }
        }
        else
        {
            // Win32LogError("Get File Size For: %s Failed!", FileName);
        }
        
        CloseHandle(FileHandle);
    }
    else
    {
        // Win32LogError("The File: %s Does Not Exist!", FileName);
    }
    
    return Result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
    b32 Result = false;
    
    HANDLE FileHandle = CreateFileA(FileName,
                                    GENERIC_WRITE, 0,
                                    0,
                                    CREATE_ALWAYS,
                                    0, 0);
    
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten;
        
        if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
        {
            // NOTE(yuval): The file was read successfully
            Result = (BytesWritten == MemorySize);
        }
        else
        {
            // Win32LogError("Writing To File: %s Failed!", FileName);
        }
        
        CloseHandle(FileHandle);
    }
    else
    {
        // Win32LogError("Could Not Create Or Overwrite File: %s!", FileName);
    }
    
    return Result;
}

internal void
Win32ToggleFullscreen(HWND Window)
{
    DWORD Style = GetWindowLongA(Window, GWL_STYLE);
    
    if (Style & WS_OVERLAPPEDWINDOW)
    {
        MONITORINFO MonitorInfo = { sizeof(MONITORINFO) };
        
        if (GetWindowPlacement(Window, &GlobalWindowPosition) &&
            GetMonitorInfoA(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY),
                            &MonitorInfo))
        {
            SetWindowLongA(Window, GWL_STYLE, Style & -WS_OVERLAPPEDWINDOW);
            SetWindowPos(Window, HWND_TOP,
                         MonitorInfo.rcMonitor.left,
                         MonitorInfo.rcMonitor.top,
                         MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                         MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {
        SetWindowLongA(Window, GWL_STYLE, Style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(Window, &GlobalWindowPosition);
        SetWindowPos(Window, HWND_TOP,
                     0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

internal void
Win32BuildEXEPathFileName(char* Dest, memory_index DestCount,
                          win32_state* State, const char* FileName)
{
    CatStrings(Dest, DestCount,
               State->EXEFileName,
               State->OnePastLastEXEFileNameSlash - State->EXEFileName,
               FileName, StringLength(FileName));
}

internal void
Win32GetInputFileLocation(char* Dest, memory_index DestCount,
                          b32 IsInputStream, win32_state* State,
                          s32 SlotIndex)
{
    char Temp[64];
    _snprintf_s(Temp, sizeof(Temp), "loop_edit_%d_%s.gi",
                SlotIndex, IsInputStream ? "input" : "state");
    
    Win32BuildEXEPathFileName(Dest, DestCount, State, Temp);
}

internal win32_replay_buffer*
Win32GetReplayBuffer(win32_state* State, u32 Index)
{
    Assert(Index < ArrayCount(State->ReplayBuffers));
    win32_replay_buffer* Result = &State->ReplayBuffers[Index];
    return Result;
}

internal void
Win32BeginRecordingInput(win32_state* State, s32 InputRecordingIndex)
{
    win32_replay_buffer* ReplayBuffer = Win32GetReplayBuffer(State, InputRecordingIndex - 1);
    
    if (ReplayBuffer->MemoryBlock)
    {
        State->InputRecordingIndex = InputRecordingIndex;
        
        char FileName[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation(FileName, sizeof(FileName),
                                  true, State, InputRecordingIndex);
        
        State->RecordingHandle = CreateFileA(FileName, GENERIC_WRITE,
                                             0, 0, CREATE_ALWAYS, 0, 0);
        
        CopyMemory(ReplayBuffer->MemoryBlock, State->GameMemoryBlock, State->TotalSize);
    }
}

internal void
Win32EndRecordingInput(win32_state* State)
{
    CloseHandle(State->RecordingHandle);
    State->InputRecordingIndex = 0;
}

internal void
Win32BeginInputPlayBack(win32_state* State, s32 InputPlayingIndex)
{
    win32_replay_buffer* ReplayBuffer = Win32GetReplayBuffer(State, InputPlayingIndex - 1);
    
    if (ReplayBuffer->MemoryBlock)
    {
        State->InputPlayingIndex = InputPlayingIndex;
        
        char FileName[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation(FileName, sizeof(FileName),
                                  true, State, InputPlayingIndex);
        
        State->PlayBackHandle = CreateFileA(FileName, GENERIC_READ,
                                            FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        
        CopyMemory(State->GameMemoryBlock, ReplayBuffer->MemoryBlock, State->TotalSize);
        
    }
}

internal void
Win32EndInputPlayBack(win32_state* State)
{
    CloseHandle(State->PlayBackHandle);
    State->InputPlayingIndex = 0;
}


internal void
Win32RecordInput(win32_state* State, game_input* NewInput)
{
    DWORD BytesWritten;
    WriteFile(State->RecordingHandle, NewInput,
              sizeof(*NewInput), &BytesWritten, 0);
    
}

internal void
Win32PlayBackInput(win32_state* State, game_input* NewInput)
{
    DWORD BytesRead;
    ReadFile(State->PlayBackHandle, NewInput,
             sizeof(*NewInput), &BytesRead, 0);
    
    if (!BytesRead)
    {
        s32 PlayingIndex = State->InputPlayingIndex;
        Win32EndInputPlayBack(State);
        Win32BeginInputPlayBack(State, PlayingIndex);
    }
}

#if 0
internal void
DEBUGWin32DrawVertical(win32_offscreen_buffer* Backbuffer, s32 X,
                       s32 Top, s32 Bottom, u32 Color)
{
    if (Top < 0)
    {
        Top = 0;
    }
    
    if (Bottom > Backbuffer->Height)
    {
        Bottom = Backbuffer->Height;
    }
    
    if ((X >= 0) && (X < Backbuffer->Width))
    {
        u8* Pixel = ((u8*)Backbuffer->Memory +
                     X * Backbuffer->BytesPerPixel +
                     Top * Backbuffer->Pitch);
        
        for (s32 Y = Top; Y < Bottom; ++Y)
        {
            *(u32*)Pixel = Color;
            Pixel += Backbuffer->Pitch;
        }
    }
}

inline void
DEBUGWin32DrawSoundBufferMarker(win32_offscreen_buffer* Backbuffer,
                                win32_sound_output* SoundOutput,
                                f32 C, s32 PadX, s32 Top, s32 Bottom,
                                DWORD Value, u32 Color)
{
    s32 X = PadX + (s32)((f32)Value * C);
    DEBUGWin32DrawVertical(Backbuffer, X, Top, Bottom, Color);
}

internal void
DEBUGWin32SyncDisplay(win32_offscreen_buffer* Backbuffer,
                      win32_debug_time_marker* Markers, u32 MarkerCount,
                      u32 CurrentMarkerIndex, win32_sound_output* SoundOutput,
                      f32 TargetSecondsPerFrame)
{
    u32 PlayColor = 0xFFFFFFFF;
    u32 WriteColor = 0xFFFF0000;
    u32 ExpectedFlipColor = 0xFFFFFF00;
    u32 PlayWindowColor = 0xFFFF00FF;
    
    s32 PadX = 16;
    s32 PadY = 16;
    
    s32 LineHeight = 64;
    
    f32 C = (f32)(Backbuffer->Width - PadX * 2) / (f32)SoundOutput->SecondaryBufferSize;
    
    for (u32 MarkerIndex = 0;
         MarkerIndex < MarkerCount;
         ++MarkerIndex)
    {
        s32 Top = PadY;
        s32 Bottom = PadY + LineHeight;
        
        win32_debug_time_marker* ThisMarker = &Markers[MarkerIndex];
        Assert(ThisMarker->OutputPlayCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputWriteCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputLocation < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputByteCount <= SoundOutput->SecondaryBufferSize);
        
        Assert(ThisMarker->FlipPlayCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->FlipWriteCursor < SoundOutput->SecondaryBufferSize);
        
        if (MarkerIndex == CurrentMarkerIndex)
        {
            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;
            
            s32 FirstTop = Top;
            
            DEBUGWin32DrawSoundBufferMarker(Backbuffer, SoundOutput,
                                            C, PadX, Top, Bottom,
                                            ThisMarker->OutputPlayCursor, PlayColor);
            
            DEBUGWin32DrawSoundBufferMarker(Backbuffer, SoundOutput,
                                            C, PadX, Top, Bottom,
                                            ThisMarker->OutputWriteCursor, WriteColor);
            
            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;
            
            DEBUGWin32DrawSoundBufferMarker(Backbuffer, SoundOutput,
                                            C, PadX, Top, Bottom,
                                            ThisMarker->OutputLocation, PlayColor);
            
            DEBUGWin32DrawSoundBufferMarker(Backbuffer, SoundOutput,
                                            C, PadX, Top, Bottom,
                                            ThisMarker->OutputLocation + ThisMarker->OutputByteCount,
                                            PlayColor);
            
            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;
            
            DEBUGWin32DrawSoundBufferMarker(Backbuffer, SoundOutput,
                                            C, PadX, FirstTop, Bottom,
                                            ThisMarker->ExpectedFlipPlayCursor, ExpectedFlipColor);
            
        }
        
        DEBUGWin32DrawSoundBufferMarker(Backbuffer, SoundOutput,
                                        C, PadX, Top, Bottom,
                                        ThisMarker->FlipPlayCursor, PlayColor);
        
        DEBUGWin32DrawSoundBufferMarker(Backbuffer, SoundOutput,
                                        C, PadX, Top, Bottom,
                                        ThisMarker->FlipPlayCursor + 480 * SoundOutput->BytesPerSample,
                                        PlayWindowColor);
        
        DEBUGWin32DrawSoundBufferMarker(Backbuffer, SoundOutput,
                                        C, PadX, Top, Bottom,
                                        ThisMarker->FlipWriteCursor, WriteColor);
    }
}
#endif

internal LARGE_INTEGER
Win32GetWallClock()
{
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return Result;
}

internal f32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    f32 Result = ((f32)(End.QuadPart - Start.QuadPart) /
                  (f32)GlobalPerfCountFrequency);
    return Result;
}

internal void
Win32ProcessKeyboardMessage(game_button_state* NewState, b32 IsDown)
{
    //Assert(NewState->EndedDown != IsDown);
    NewState->EndedDown = IsDown;
    ++NewState->HalfTransitionCount;
}

internal f32
Win32NormalizeXInputStick(s16 StickValue, const s16 DeadZoneThreshold)
{
    f32 Result = 0;
    
    if (StickValue < -DeadZoneThreshold)
    {
        Result = (f32)StickValue / 32768.0f;
    }
    else if (StickValue > DeadZoneThreshold)
    {
        Result = (f32)StickValue / 32767.0f;
    }
    
    return Result;
}

internal void
Win32ProcessXInputDigitalButton(DWORD XInputButtonState,
                                DWORD ButtonBit,
                                game_button_state* OldState,
                                game_button_state* NewState)
{
    NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal void
Win32FillSoundBuffer(IDirectSoundBuffer* SoundBuffer,
                     game_sound_output_buffer* SourceBuffer,
                     win32_sound_output* SoundOutput,
                     DWORD ByteToLock, DWORD BytesToWrite)
{
    void* Region1;
    DWORD Region1Size;
    void* Region2;
    DWORD Region2Size;
    
    // NOTE(yuval): Locking the buffer before writing
    if (SUCCEEDED(SoundBuffer->Lock(ByteToLock, BytesToWrite,
                                    &Region1, &Region1Size,
                                    &Region2, &Region2Size,
                                    0)))
    {
        // TODO(yuval): Assert that Region1Size & Region2Size are
        // an even multiply of the samples
        
        s16* SourceSample = SourceBuffer->Samples;
        
        // NOTE: Filling Region 1
        u32 Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        s16* DestSample = (s16*)Region1;
        
        for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }
        
        // NOTE: Filling Region 2
        u32 Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        DestSample = (s16*)Region2;
        for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }
        
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
    else
    {
        // TODO(yuval & eran): Better log
        //Win32LogError("Buffer Locking Failed!");
    }
}

internal void
Win32ClearSoundBuffer(IDirectSoundBuffer* SoundBuffer,
                      win32_sound_output* SoundOutput)
{
    void* Region1;
    DWORD Region1Size;
    void* Region2;
    DWORD Region2Size;
    
    // NOTE(yuval): Locking the buffer before writing
    if (SUCCEEDED(SoundBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
                                    &Region1, &Region1Size,
                                    &Region2, &Region2Size,
                                    0)))
    {
        Assert(!Region2Size);
        
        s8* DestSample = (s8*)Region1;
        for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex)
        {
            *DestSample++ = 0;
        }
        
        DestSample = (s8*)Region2;
        for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex)
        {
            *DestSample++ = 0;
        }
        
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size,
                                      Region2, Region2Size);
    }
}

internal void
Win32InitDSound(HWND Window, s32 SamplesPerSecond, s32 BufferSize)
{
    // NOTE(yuval): Loading the direct sound library
    HMODULE DSoundLibrary = LoadLibrary("dsound.dll");
    
    // NOTE(yuval): Creating a DirectSound object
    if (DSoundLibrary)
    {
        // NOTE(yuval): Loading the DirectSoundCreate function
        direct_sound_create* DirectSoundCreate = (direct_sound_create*)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
        
        // NOTE(yuval): Creating the DirectSound object
        IDirectSound* DirectSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            
            WAVEFORMATEX WaveFormat = { };
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;
            
            // NOTE(yuval): Setting the cooperative level to PRIORITY
            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window,
                                                           DSSCL_PRIORITY)))
            {
                // NOTE(yuval): Creating the primary buffer
                DSBUFFERDESC BufferDescription = { };
                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
                
                IDirectSoundBuffer* PrimaryBuffer;
                
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription,
                                                             &PrimaryBuffer, 0)))
                {
                    // NOTE(yuval): Setting the buffer format
                    if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
                    {
                        // NOTE(yuval): Finished setting the format
                        
                        // TODO(yuval & eran): LogInfo("Primary buffer format was set");
                    }
                    else
                    {
                        // TODO(yuval & eran): LogError("Primary Buffer Formatting Failed!");
                    }
                }
            }
            else
            {
                // TODO(yuval & eran): LogError("DirectSound Cooperatrive Level Setting Failed!");
            }
            
            // NOTE(yuval): Creating the secondery buffer
            DSBUFFERDESC BufferDescription = { };
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwFlags = 0;
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;
            
            if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription,
                                                         &GlobalSecondaryBuffer, 0)))
            {
                // TODO(yuval & eran): LogInfo("The secondery sound buffer was created");
            }
        }
        else
        {
            // TODO(yuval & eran): LogError("DirectSound Object Creation Failed!");
        }
    }
    else
    {
        // TODO(yuval & eran): LogError("dsound.dll Loading Failed!");
    }
}

internal void
Win32LoadXInput()
{
    // NOTE(yuval): Trying to load xinput1_4
    HMODULE XInputLibrary = LoadLibrary("xinput1_4.dll");
    
    // NOTE(yuval): It it doesn't succeed, trying to load 1_3
    if (!XInputLibrary)
    {
        LoadLibrary("xinput1_3.dll");
    }
    
    if (XInputLibrary)
    {
        XInputGetState = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
        
        if (!XInputGetState)
        {
            XInputGetState = XInputGetStateStub;
        }
        
        XInputSetState = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
        
        if (!XInputGetState)
        {
            XInputSetState = XInputSetStateStub;
        }
    }
    else
    {
        // TODO(yuval & eran): LogError("Failed To Load xinput1_4.dll and xinput1_3.dll!");
    }
}

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Dim = { };
    
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    
    // NOTE(yuval & eran): Duplicated code, refactor into function
    Dim.Width = ClientRect.right - ClientRect.left;
    Dim.Height = ClientRect.bottom - ClientRect.top;
    
    return Dim;
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer* Buffer, s32 Width, s32 Height)
{
    if (Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }
    
    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->BytesPerPixel = 4;
    
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;
    
    s32 BitmapMemorySize = Buffer->Width * Buffer->Height * Buffer->BytesPerPixel;
    
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize,
                                  MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    
    Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;
}

internal void
Win32DisplayBackbufferInWindow(HDC DeviceContext, win32_offscreen_buffer* Buffer,
                               s32 WindowWidth, s32 WindowHeight)
{
    if ((WindowWidth == Buffer->Width * 2) &&
        (WindowHeight == Buffer->Height * 2))
    {
        StretchDIBits(DeviceContext,
                      0, 0, WindowWidth, WindowHeight,
                      0, 0, Buffer->Width, Buffer->Height,
                      Buffer->Memory,
                      &Buffer->Info,
                      DIB_RGB_COLORS,
                      SRCCOPY);
    }
    else
    {
        StretchDIBits(DeviceContext,
                      0, 0, Buffer->Width, Buffer->Height,//WindowWidth, WindowHeight,
                      0, 0, Buffer->Width, Buffer->Height,
                      Buffer->Memory,
                      &Buffer->Info,
                      DIB_RGB_COLORS,
                      SRCCOPY);
    }
}

internal void
Win32ProcessPendingMessages(win32_state* State, game_controller* KeyboardController)
{
    MSG Message;
    
    while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
    {
        switch (Message.message)
        {
            case WM_QUIT:
            {
                GlobalRunning = false;
            } break;
            
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                WPARAM KeyCode = Message.wParam;
                bool WasDown = (Message.lParam & (1 << 30)) != 0;
                bool IsDown = (Message.lParam & (1 << 31)) == 0;
                
                b32 AltKeyIsDown = (Message.lParam & (1 << 29));
                
                if (KeyCode == VK_F4 && AltKeyIsDown)
                {
                    GlobalRunning = false;
                }
#if GAME_INTERNAL
                else if (KeyCode == VK_F11 && AltKeyIsDown)
                {
                    if (IsDown)
                    {
                        if (Message.hwnd)
                        {
                            Win32ToggleFullscreen(Message.hwnd);
                        }
                    }
                }
#endif
                else if (WasDown != IsDown)
                {
                    switch (KeyCode)
                    {
                        case 'W':
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                        } break;
                        
                        case 'A':
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
                        } break;
                        
                        case 'S':
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
                        } break;
                        
                        case 'D':
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
                        } break;
                        
                        case 'Q':
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
                        } break;
                        
                        case 'E':
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
                        } break;
                        
                        case VK_UP:
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
                        } break;
                        
                        case VK_DOWN:
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
                        } break;
                        
                        case VK_LEFT:
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
                        } break;
                        
                        case VK_RIGHT:
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
                        } break;
                        
                        case VK_ESCAPE:
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
                        } break;
                        
                        case VK_SPACE:
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
                        } break;
                        
                        case VK_SHIFT:
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->Run, IsDown);
                        } break;
                        
#if GAME_INTERNAL
                        case 'P':
                        {
                            if (IsDown)
                            {
                                GlobalPause = !GlobalPause;
                            }
                        } break;
                        
                        case 'L':
                        {
                            if (IsDown)
                            {
                                if (State->InputPlayingIndex == 0)
                                {
                                    if (State->InputRecordingIndex == 0)
                                    {
                                        Win32BeginRecordingInput(State, 1);
                                    }
                                    else
                                    {
                                        Win32EndRecordingInput(State);
                                        Win32BeginInputPlayBack(State, 1);
                                    }
                                }
                                else
                                {
                                    Win32EndInputPlayBack(State);
                                    
                                    // TODO(yuval & eran): Zero the whole input
                                    // NOT JUST THE KEYBOARD CONTROLLER
                                    game_controller ZeroController = { };
                                    *KeyboardController = ZeroController;
                                }
                            }
                        }
#endif
                    }
                }
            } break;
            
            default:
            {
                TranslateMessage(&Message);
                DispatchMessageA(&Message);
            } break;
        }
    }
    
}
internal LRESULT CALLBACK
Win32MainWindowCallback(HWND Window, UINT Msg,
                        WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;
    
    switch (Msg)
    {
        case WM_SIZE:
        {
            
        } break;
        
        case WM_SETCURSOR:
        {
            if (!GlobalShowCursor)
            {
                SetCursor(0);
            }
        } break;
        
        case WM_DESTROY:
        {
            GlobalRunning = false;
        } break;
        
        case WM_CLOSE:
        {
            GlobalRunning = false;
        } break;
        
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            SoftAssert(!"Keyboard Input Came In Through The Main Window Callback!");
        } break;
        
        case WM_ACTIVATEAPP:
        {
            // TODO(yuval & eran): LogDebug("Window Refocused");
        } break;
        
        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            
            win32_window_dimension Dim = Win32GetWindowDimension(Window);
            Win32DisplayBackbufferInWindow(DeviceContext, &GlobalBackbuffer, Dim.Width, Dim.Height);
            
            EndPaint(Window, &Paint);
        } break;
        
        default:
        {
            Result = DefWindowProc(Window, Msg, WParam, LParam);
        } break;
    }
    
    return Result;
}

internal void
Win32UnloadGameCode(win32_game_code* GameCode)
{
    if (GameCode->GameCodeDLL)
    {
        FreeLibrary(GameCode->GameCodeDLL);
        GameCode->GameCodeDLL = 0;
    }
    
    GameCode->IsValid = false;
    GameCode->UpdateAndRender = 0;
    GameCode->GetSoundSamples = 0;
}

inline FILETIME
Win32GetLastWriteTime(const char* FileName)
{
    FILETIME LastWriteTime = { };
    
    WIN32_FILE_ATTRIBUTE_DATA Data;
    if (GetFileAttributesExA(FileName, GetFileExInfoStandard, &Data))
    {
        LastWriteTime = Data.ftLastWriteTime;
    }
    
    return LastWriteTime;
}

internal win32_game_code
Win32LoadGameCode(const char* SourceGameCodeDLLFullPath,
                  const char* TempGameCodeDLLFullPath,
                  const char* LockFileName)
{
    win32_game_code Result = { };
    
    WIN32_FILE_ATTRIBUTE_DATA Ignored;
    if (!GetFileAttributesExA(LockFileName, GetFileExInfoStandard, &Ignored))
    {
        Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
        
        CopyFileA(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath, FALSE);
        Result.GameCodeDLL = LoadLibrary(TempGameCodeDLLFullPath);
        
        if (Result.GameCodeDLL)
        {
            Result.UpdateAndRender = (game_update_and_render*)
                GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");
            Result.GetSoundSamples = (game_get_sound_samples*)
                GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");
            
            Result.IsValid = (Result.UpdateAndRender &&
                              Result.GetSoundSamples);
        }
    }
    
    if (!Result.IsValid)
    {
        Result.UpdateAndRender = 0;
        Result.GetSoundSamples = 0;
    }
    
    return Result;
}

internal void
Win32GetEXEFileName(win32_state* State)
{
    DWORD SizeOfFileName = GetModuleFileNameA(0, State->EXEFileName,
                                              sizeof(State->EXEFileName));
    
    State->OnePastLastEXEFileNameSlash = State->EXEFileName + SizeOfFileName;
    
    for (char* Scan = State->EXEFileName; *Scan; ++Scan)
    {
        if (*Scan == '\\' || *Scan == '/')
        {
            State->OnePastLastEXEFileNameSlash = Scan + 1;
        }
    }
}

s32 WINAPI
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        s32 ShowCode)
{
    Platform.DisplayMessageBox = PlatformDisplayMessageBox;
    Platform.GetDateTime = PlatformGetDateTime;
    Platform.DEBUGFreeFileMemory =  DEBUGPlatformFreeFileMemory;
    Platform.DEBUGReadEntireFile = DEBUGPlatformReadEntireFile;
    Platform.DEBUGWriteEntireFile = DEBUGPlatformWriteEntireFile;
    
    win32_state Win32State = { };
    
    Win32GetEXEFileName(&Win32State);
    
    char SourceGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(SourceGameCodeDLLFullPath,
                              sizeof(SourceGameCodeDLLFullPath),
                              &Win32State, "game.dll");
    
    char TempGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(TempGameCodeDLLFullPath,
                              sizeof(TempGameCodeDLLFullPath),
                              &Win32State, "game_temp.dll");
    
    char GameCodeLockFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(GameCodeLockFullPath,
                              sizeof(GameCodeLockFullPath),
                              &Win32State, "lock.tmp");
    
    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;
    
    UINT DesiredSchedulerMS = 1;
    b32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);
    
    Win32LoadXInput();
    
    s32 RenderWidth = 960;
    s32 RenderHeight = 540;
    Win32ResizeDIBSection(&GlobalBackbuffer, RenderWidth, RenderHeight);
    
    WNDCLASS WindowClass = { };
    
    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    // HICON     hIcon;
    WindowClass.lpszClassName = "HandmadeGameWindowClass";
    // WindowClass.hCursor = LoadCursorA(Instance, IDC_CROSS);
    
    if (RegisterClassA(&WindowClass))
    {
        HWND Window = CreateWindowExA(
            0, // WS_EX_TOPMOST | WS_EX_LAYERED,
            WindowClass.lpszClassName,
            "Handmade Game",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            Instance,
            0);
        
        if (Window)
        {
#if GAME_INTERNAL
            GlobalShowCursor = true;
#else
            GlobalShowCursor = false;
            Win32ToggleFullscreen(Window);
#endif
            
            // TODO(yuval & eran): Use this in future developer mode
            // SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 255, LWA_ALPHA);
            
            s32 MonitorRefreshRate = 60;
            
            HDC RefreshDC = GetDC(Window);
            s32 Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
            ReleaseDC(Window, RefreshDC);
            
            if (Win32RefreshRate > 1)
            {
                OutputDebugStringA("Windows Found A Fefresh Rate\n");
                MonitorRefreshRate = Win32RefreshRate;
            }
            
            char TextBuffer[256];
            FormatString(TextBuffer, sizeof(TextBuffer),
                         "Refresh Rate:%d\n", MonitorRefreshRate);
            OutputDebugStringA(TextBuffer);
            
            f32 GameUpdateHz = MonitorRefreshRate / 2.0f;
            f32 TargetSecondsPerFrame = 1.0f / GameUpdateHz;
            
            win32_sound_output SoundOutput = { };
            
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.BytesPerSample = sizeof(s16) * 2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond *
                SoundOutput.BytesPerSample;
            
            // TODO(yuval & eran): Find what the lowest possible safeBytes value is
            SoundOutput.SafetyBytes =
                (DWORD)(((f32)(SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) /
                         GameUpdateHz) / 3.0f);
            
            Win32InitDSound(Window, SoundOutput.SamplesPerSecond,
                            SoundOutput.SecondaryBufferSize);
            
            Win32ClearSoundBuffer(GlobalSecondaryBuffer, &SoundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
            
#if 0
            // NOTE(yuval): This tests the Play / Write cursors' update frequency
            // on this machine (HP ProBook), it was 480 samples
            while (true)
            {
                DWORD PlayCursor;
                DWORD WriteCursor;
                GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);
                
                char TextBuffer[256];
                _snprintf_s(TextBuffer, sizeof(TextBuffer),
                            "PC:%u WC:%u\n", PlayCursor, WriteCursor);
                OutputDebugStringA(TextBuffer);
            }
#endif
            
            s16* Samples = (s16*)VirtualAlloc(0, SoundOutput.SecondaryBufferSize,
                                              MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            
#if GAME_INTERNAL
            void* BaseAddress = (void*)Terabytes(2);
#else
            void* BaseAddress = 0;
#endif
            
            game_memory GameMemory = { };
            GameMemory.PermanentStorageSize = Megabytes(64);
            GameMemory.TransientStorageSize = Gigabytes(1);
            
            GameMemory.PlatformAPI = Platform;
            
            Win32State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
            
            Win32State.GameMemoryBlock = VirtualAlloc(BaseAddress, Win32State.TotalSize,
                                                      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            
            GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
            GameMemory.TransientStorage = (u8*)GameMemory.PermanentStorage +
                GameMemory.PermanentStorageSize;
            
            for (s32 ReplayIndex = 0;
                 ReplayIndex < ArrayCount(Win32State.ReplayBuffers);
                 ++ReplayIndex)
            {
                win32_replay_buffer* ReplayBuffer = &Win32State.ReplayBuffers[ReplayIndex];
                
                Win32GetInputFileLocation(ReplayBuffer->ReplayFileName,
                                          sizeof(ReplayBuffer->ReplayFileName),
                                          false, &Win32State, ReplayIndex + 1);
                
                ReplayBuffer->FileHandle = CreateFileA(ReplayBuffer->ReplayFileName, GENERIC_READ | GENERIC_WRITE,
                                                       0, 0, CREATE_ALWAYS, 0, 0);
                
                // TODO(yuval & eran): @Refactor lowhigh conversions to macros
                DWORD MaxSizeHigh = (Win32State.TotalSize >> 32);
                DWORD MaxSizeLow = (Win32State.TotalSize & 0xFFFFFFFF);
                
                ReplayBuffer->MemoryMap = CreateFileMappingA(ReplayBuffer->FileHandle,
                                                             0,
                                                             PAGE_READWRITE,
                                                             MaxSizeHigh,
                                                             MaxSizeLow,
                                                             0);
                
                ReplayBuffer->MemoryBlock = MapViewOfFile(ReplayBuffer->MemoryMap,
                                                          FILE_MAP_ALL_ACCESS,
                                                          0, 0, Win32State.TotalSize);
                
                if (!ReplayBuffer->MemoryBlock)
                {
                    // TODO(yuval & eran): @Diagnostic
                }
            }
            
            if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
            {
                game_input Inputs[2] = { };
                game_input* NewInput = &Inputs[0];
                game_input* OldInput = &Inputs[1];
                
                b32 SoundIsValid = false;
                DWORD AudioLatencyBytes = 0;
                f32 AudioLatencySeconds = 0;
                
                win32_debug_time_marker DEBUGTimeMarkers[15] = { };
                u32 DEBUGTimeMarkerIndex = 0;
                
                win32_game_code Game = Win32LoadGameCode(SourceGameCodeDLLFullPath,
                                                         TempGameCodeDLLFullPath,
                                                         GameCodeLockFullPath);
                
                GlobalRunning = true;
                GlobalPause = false;
                
                LARGE_INTEGER LastCounter = Win32GetWallClock();
                LARGE_INTEGER FlipWallClock = Win32GetWallClock();
                u64 LastCycleCount = __rdtsc();
                
                while (GlobalRunning)
                {
                    FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
                    
                    if (CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime))
                    {
                        Win32UnloadGameCode(&Game);
                        Game = Win32LoadGameCode(SourceGameCodeDLLFullPath,
                                                 TempGameCodeDLLFullPath,
                                                 GameCodeLockFullPath);
                    }
                    
                    game_controller* OldKeyboardController = &OldInput->Controllers[0];
                    game_controller* NewKeyboardController = &NewInput->Controllers[0];
                    
                    // TODO(yuval & eran): Zero The Struct Using A Function
                    game_controller ZeroController = { };
                    *NewKeyboardController = ZeroController;
                    NewKeyboardController->IsConnected = true;
                    NewKeyboardController->IsAnalog = false;
                    
                    for (s32 ButtonIndex = 0;
                         ButtonIndex < ArrayCount(NewKeyboardController->Buttons);
                         ++ButtonIndex)
                    {
                        NewKeyboardController->Buttons[ButtonIndex].EndedDown =
                            OldKeyboardController->Buttons[ButtonIndex].EndedDown;
                    }
                    
                    Win32ProcessPendingMessages(&Win32State, NewKeyboardController);
                    
                    if (!GlobalPause)
                    {
                        NewInput->dtForFrame = TargetSecondsPerFrame;
                        
                        POINT MouseP;
                        GetCursorPos(&MouseP);
                        ScreenToClient(Window, &MouseP);
                        
                        NewInput->MouseX = MouseP.x;
                        NewInput->MouseY = MouseP.y;
                        NewInput->MouseZ = 0;
                        
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[0],
                                                    GetKeyState(VK_LBUTTON) & (1 << 15));
                        
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[1],
                                                    GetKeyState(VK_MBUTTON) & (1 << 15));
                        
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[2],
                                                    GetKeyState(VK_RBUTTON) & (1 << 15));
                        
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[3],
                                                    GetKeyState(VK_XBUTTON1) & (1 << 15));
                        
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[4],
                                                    GetKeyState(VK_XBUTTON2) & (1 << 15));
                        
                        DWORD MaxControllerCount = XUSER_MAX_COUNT + 1;
                        
                        if (MaxControllerCount > ArrayCount(NewInput->Controllers))
                        {
                            MaxControllerCount = ArrayCount(NewInput->Controllers);
                        }
                        
                        for (DWORD ControllerIndex = 1;
                             ControllerIndex < MaxControllerCount;
                             ++ControllerIndex)
                        {
                            XINPUT_STATE ControllerState;
                            
                            game_controller* OldController = &OldInput->Controllers[ControllerIndex];
                            game_controller* NewController = &NewInput->Controllers[ControllerIndex];
                            
                            if (XInputGetState(ControllerIndex - 1, &ControllerState) == ERROR_SUCCESS)
                            {
                                NewController->IsConnected = true;
                                NewController->IsAnalog = OldController->IsAnalog;
                                
                                XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;
                                
                                // NOTE(yuval): Stick Processing
                                NewController->StickAverageX =
                                    Win32NormalizeXInputStick(Pad->sThumbLX,
                                                              XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                
                                
                                NewController->StickAverageY =
                                    Win32NormalizeXInputStick(Pad->sThumbLY,
                                                              XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                
                                f32 Threshold = 0.5f;
                                
                                // NOTE(yuval): Stick X Fake Digital Button Processing
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageX < -Threshold) ? 1 : 0,
                                    1,
                                    &OldController->MoveLeft,
                                    &NewController->MoveLeft);
                                
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageX > Threshold) ? 1 : 0,
                                    1,
                                    &OldController->MoveRight,
                                    &NewController->MoveRight);
                                
                                // NOTE(yuval): Stick Y Fake Digital Button Processing
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageY < -Threshold) ? 1 : 0,
                                    1,
                                    &OldController->MoveDown,
                                    &NewController->MoveDown);
                                
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageY > Threshold) ? 1 : 0,
                                    1,
                                    &OldController->MoveUp,
                                    &NewController->MoveUp);
                                
                                // NOTE(yuval): DPAD Processing
                                // NOTE(yuval): DPAD Processing as fake stick
                                if (NewController->StickAverageX != 0 ||
                                    NewController->StickAverageY != 0)
                                {
                                    NewController->IsAnalog = true;
                                }
                                
                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                                {
                                    NewController->StickAverageY = 1.0f;
                                    NewController->IsAnalog = false;
                                }
                                
                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                                {
                                    NewController->StickAverageY = -1.0f;
                                    NewController->IsAnalog = false;
                                }
                                
                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                                {
                                    NewController->StickAverageX = -1.0f;
                                    NewController->IsAnalog = false;
                                }
                                
                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                                {
                                    NewController->StickAverageX = 1.0f;
                                    NewController->IsAnalog = false;
                                }
                                
                                // NOTE(yuval): DPAD Processing as digital button
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                XINPUT_GAMEPAD_DPAD_UP,
                                                                &OldController->MoveUp,
                                                                &NewController->MoveUp);
                                
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                XINPUT_GAMEPAD_DPAD_DOWN,
                                                                &OldController->MoveDown,
                                                                &NewController->MoveDown);
                                
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                XINPUT_GAMEPAD_DPAD_LEFT,
                                                                &OldController->MoveLeft,
                                                                &NewController->MoveLeft);
                                
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                XINPUT_GAMEPAD_DPAD_RIGHT,
                                                                &OldController->MoveRight,
                                                                &NewController->MoveRight);
                                
                                // NOTE(yuval): Digital Button Processing
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                XINPUT_GAMEPAD_Y,
                                                                &OldController->ActionUp,
                                                                &NewController->ActionUp);
                                
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                XINPUT_GAMEPAD_Y,
                                                                &OldController->ActionUp,
                                                                &NewController->ActionUp);
                                
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                XINPUT_GAMEPAD_A,
                                                                &OldController->ActionDown,
                                                                &NewController->ActionDown);
                                
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                XINPUT_GAMEPAD_X,
                                                                &OldController->ActionLeft,
                                                                &NewController->ActionLeft);
                                
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                XINPUT_GAMEPAD_B,
                                                                &OldController->ActionRight,
                                                                &NewController->ActionRight);
                                
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                XINPUT_GAMEPAD_LEFT_SHOULDER,
                                                                &OldController->LeftShoulder,
                                                                &NewController->LeftShoulder);
                                
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                                                &OldController->RightShoulder,
                                                                &NewController->RightShoulder);
                                
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                XINPUT_GAMEPAD_START,
                                                                &OldController->Start,
                                                                &NewController->Start);
                                
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                XINPUT_GAMEPAD_BACK,
                                                                &OldController->Back,
                                                                &NewController->Back);
                            }
                            else
                            {
                                NewController->IsConnected = false;
                            }
                        }
                        
                        thread_context Thread = { };
                        
                        game_offscreen_buffer OffscreenBuffer = { };
                        OffscreenBuffer.Memory = GlobalBackbuffer.Memory;
                        OffscreenBuffer.Width = GlobalBackbuffer.Width;
                        OffscreenBuffer.Height = GlobalBackbuffer.Height;
                        OffscreenBuffer.Pitch = GlobalBackbuffer.Pitch;
                        OffscreenBuffer.BytesPerPixel = GlobalBackbuffer.BytesPerPixel;
                        
                        if (Win32State.InputRecordingIndex)
                        {
                            Win32RecordInput(&Win32State, NewInput);
                        }
                        
                        if (Win32State.InputPlayingIndex)
                        {
                            Win32PlayBackInput(&Win32State, NewInput);
                        }
                        
                        if (Game.UpdateAndRender)
                        {
                            Game.UpdateAndRender(&Thread, &GameMemory, NewInput, &OffscreenBuffer);
                        }
                        
                        LARGE_INTEGER AudioCounter = Win32GetWallClock();
                        f32 FromBeginToAudioSeconds = Win32GetSecondsElapsed(FlipWallClock, AudioCounter);
                        
                        DWORD PlayCursor;
                        DWORD WriteCursor;
                        if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                        {
                            /* NOTE(yuval): Sound Output Computation
                            When we wake up to write audio, we will loop
                            and see what the play cursor position is and
                            we will forecast ahead to where play cursor
                            will be in the next frame boundary.
                            
                            We will look to see if the write cursor
                            is before the next frame boundary.
                            
                            If it is, the target fill position is the
                            frame boundary. This gives us perfect audio
                            sync in the case of a cart that has a low
                            audio latency.
                            
                            If not, then we assume we never sync the
                            audio perfectly. So we will write one
                            frame's of audio plus some number of guard
                            samples.
                            (1ms, or something determined to be safe).
                            */
                            
                            if (!SoundIsValid)
                            {
                                SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
                                SoundIsValid = true;
                            }
                            
                            DWORD ByteToLock =
                                (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample)
                                % SoundOutput.SecondaryBufferSize;
                            
                            DWORD ExpectedSoundBytesPerFrame =
                                (DWORD)((SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) /
                                        GameUpdateHz);
                            
                            f32 SecondsLeftUntilFlip = (TargetSecondsPerFrame - FromBeginToAudioSeconds);
                            
                            DWORD ExpectedBytesUntilFlip =
                                (DWORD)((SecondsLeftUntilFlip / TargetSecondsPerFrame) *
                                        ExpectedSoundBytesPerFrame);
                            
                            DWORD ExpectedFrameBoundaryByte = (PlayCursor + ExpectedBytesUntilFlip);
                            
                            DWORD SafeWriteCursor = WriteCursor;
                            if (SafeWriteCursor < PlayCursor)
                            {
                                SafeWriteCursor += SoundOutput.SecondaryBufferSize;
                            }
                            Assert(SafeWriteCursor >= PlayCursor);
                            SafeWriteCursor += SoundOutput.SafetyBytes;
                            
                            b32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);
                            
                            DWORD TargetCursor = 0;
                            if (AudioCardIsLowLatency)
                            {
                                TargetCursor = (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame);
                            }
                            else
                            {
                                TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame +
                                                SoundOutput.SafetyBytes);
                            }
                            TargetCursor %= SoundOutput.SecondaryBufferSize;
                            
                            DWORD BytesToWrite = 0;
                            if (ByteToLock > TargetCursor)
                            {
                                BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
                                BytesToWrite += TargetCursor;
                            }
                            else
                            {
                                BytesToWrite = TargetCursor - ByteToLock;
                            }
                            
                            game_sound_output_buffer SoundBuffer = { };
                            SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                            SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                            SoundBuffer.Samples = Samples;
                            
                            if (Game.GetSoundSamples)
                            {
                                Game.GetSoundSamples(&Thread, &GameMemory, &SoundBuffer);
                            }
                            
#if GAME_INTERNAL
                            win32_debug_time_marker* Marker = &DEBUGTimeMarkers[DEBUGTimeMarkerIndex];
                            Marker->OutputPlayCursor = PlayCursor;
                            Marker->OutputWriteCursor = WriteCursor;
                            Marker->OutputLocation = ByteToLock;
                            Marker->OutputByteCount = BytesToWrite;
                            Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;
                            
                            DWORD unwrappedWriteCursor = WriteCursor;
                            if (WriteCursor < PlayCursor)
                            {
                                unwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
                            }
                            
                            AudioLatencyBytes = unwrappedWriteCursor - PlayCursor;
                            
                            AudioLatencySeconds =
                                (((f32)AudioLatencyBytes / (f32)SoundOutput.BytesPerSample) /
                                 (f32)SoundOutput.SamplesPerSecond);
                            
#if 0 //GAME_INTERNAL
                            Win32LogDebug("BTL:%u, TC:%u, BTW:%u - PC:%u WC:%u DELTA:%u (%fs)",
                                          ByteToLock, TargetCursor,
                                          BytesToWrite, PlayCursor,
                                          WriteCursor,
                                          AudioLatencyBytes,
                                          AudioLatencySeconds);
#endif
                            
#endif
                            
                            Win32FillSoundBuffer(GlobalSecondaryBuffer,
                                                 &SoundBuffer, &SoundOutput,
                                                 ByteToLock, BytesToWrite);
                            
                        }
                        else
                        {
                            SoundIsValid = false;
                        }
                        
                        LARGE_INTEGER WorkCounter = Win32GetWallClock();
                        f32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);
                        
                        f32 SecondsElapsedForFrame = WorkSecondsElapsed;
                        if (SecondsElapsedForFrame < TargetSecondsPerFrame)
                        {
                            if (SleepIsGranular)
                            {
                                DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame -
                                                                   SecondsElapsedForFrame));
                                
                                if (SleepMS > 0)
                                {
                                    Sleep(SleepMS);
                                }
                                
                                SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter,
                                                                                Win32GetWallClock());
                            }
                            
                            if (SecondsElapsedForFrame > TargetSecondsPerFrame)
                            {
                                // Win32LogError("Sleep Missed Frame Rate Of %.2f\n", GameUpdateHz);
                            }
                            
                            while (SecondsElapsedForFrame < TargetSecondsPerFrame)
                            {
                                SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter,
                                                                                Win32GetWallClock());
                            }
                        }
                        else
                        {
                            // TODO(eran & yuval): MISSED FRAME RATE!
                            // Win32LogError("Frame Rate Of %.2f Missed", GameUpdateHz);
                        }
                        
                        FlipWallClock = Win32GetWallClock();
                        
#if 0 //GAME_INTERNAL
                        DEBUGWin32SyncDisplay(&GlobalBackbuffer,
                                              DEBUGTimeMarkers, ArrayCount(DEBUGTimeMarkers),
                                              DEBUGTimeMarkerIndex - 1,
                                              &SoundOutput, TargetSecondsPerFrame);
#endif
                        
                        win32_window_dimension Dim = Win32GetWindowDimension(Window);
                        HDC DeviceContext = GetDC(Window);
                        Win32DisplayBackbufferInWindow(DeviceContext, &GlobalBackbuffer, Dim.Width, Dim.Height);
                        ReleaseDC(Window, DeviceContext);
                        
                        // NOTE(yuval): This is debug code
#if GAME_INTERNAL
                        {
                            DWORD DEBUGPlayCursor;
                            DWORD DEBUGWriteCursor;
                            if (GlobalSecondaryBuffer->GetCurrentPosition(&DEBUGPlayCursor,
                                                                          &DEBUGWriteCursor) == DS_OK)
                            {
                                Assert(DEBUGTimeMarkerIndex < ArrayCount(DEBUGTimeMarkers));
                                
                                win32_debug_time_marker* Marker = &DEBUGTimeMarkers[DEBUGTimeMarkerIndex];
                                
                                Marker->FlipPlayCursor = DEBUGPlayCursor;
                                Marker->FlipWriteCursor = DEBUGWriteCursor;
                            }
                            
                            ++DEBUGTimeMarkerIndex;
                            
                            if (DEBUGTimeMarkerIndex == ArrayCount(DEBUGTimeMarkers))
                            {
                                DEBUGTimeMarkerIndex = 0;
                            }
                            
                        }
#endif
                        
                        f32 MSPerFrame = (1000.0f * Win32GetSecondsElapsed(LastCounter, FlipWallClock));
                        f32 FPS = ((f32)GlobalPerfCountFrequency /
                                   (f32)(FlipWallClock.QuadPart - LastCounter.QuadPart));
                        
                        // Win32LogDebug("%.2fms/f,  %.2ff/s", MSPerFrame, FPS);
#if 0
                        u64 EndCycleCount = __rdtsc();
                        u64 CyclesElapsed = EndCycleCount - LastCycleCount;
                        f32 MCPF = (f32)CyclesElapsed / 1000000.0f;
                        
                        // Win32LogDebug("%.2fms/f,  %.2f/s,  %.2fmc/f", MSPerFrame, FPS, MCPF);
                        
                        LastCycleCount = EndCycleCount;
#endif
                        
                        // TODO(yuval & eran): Metaprogramming SWAP
                        game_input* Temp = NewInput;
                        NewInput = OldInput;
                        OldInput = Temp;
                        
                        LastCounter = FlipWallClock;
                    }
                }
            }
            else
            {
                // TODO(yuval & eran): LogFatal("Memory Allocation Failed!");
            }
        }
        else
        {
            // TODO(yuval & eran): LogFatal("Window Creation Failed!");
        }
    }
    else
    {
        // TODO(yuval & eran): LogFatal("Window Class Registration Failed!");
    }
    
    return 0;
}
