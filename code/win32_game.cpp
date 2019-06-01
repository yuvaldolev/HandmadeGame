#include "game.h"

#include <windows.h>
#include <dsound.h>
#include <xinput.h>

#include <math.h>
#include <stdio.h> // TODO(yuvai & eran): Remove this temporary include

#include "win32_game.h"

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
#define Win32LogInternal(level, format, ...) game.Log(level, \
__FILE__, \
__FUNCTION__, \
__LINE__, \
format, __VA_ARGS__)

#define Win32LogDebug(format, ...) Win32LogInternal(LogLevelDebug, format, __VA_ARGS__)
#define Win32LogInfo(format, ...) Win32LogInternal(LogLevelInfo, format, __VA_ARGS__)
#define Win32LogWarn(format, ...) Win32LogInternal(LogLevelWarn, format, __VA_ARGS__)
#define Win32LogError(format, ...) Win32LogInternal(LogLevelError, format, __VA_ARGS__)
#define Win32LogFatal(format, ...) Win32LogInternal(LogLevelFatal, format, __VA_ARGS__)

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(XInputGetStateType);

X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}

global_variable XInputGetStateType* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(XInputSetStateType);

X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}

global_variable XInputSetStateType* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(DirectSoundCreateType);

// TODO(yuval & eran): Remove global variables!!!
global_variable b32 globalRunning;
global_variable b32 globalPause;
global_variable Win32Backbuffer globalBackbuffer;
global_variable IDirectSoundBuffer* globalSecondaryBuffer;
global_variable s64 globalPerfCountFrequency;

PLATFORM_GET_DATE_TIME(PlatformGetDateTime)
{
    SYSTEMTIME localTime;
    GetLocalTime(&localTime);
    
    PlatformDateTime result;
    result.day = localTime.wDay;
    result.month = localTime.wMonth;
    result.year = localTime.wYear;
    result.hour = localTime.wHour;
    result.minute = localTime.wMinute;
    result.second = localTime.wSecond;
    result.milliseconds = localTime.wMilliseconds;
    
    return result;
}

// TODO(yuval & eran): Temporary!
#if 0
void
PlatformWriteLogMsgInColor(LogMsg* msg)
{
    const WORD COLOR_CYAN = FOREGROUND_GREEN | FOREGROUND_BLUE;
    const WORD COLOR_GREEN = FOREGROUND_GREEN;
    const WORD COLOR_BOLD_YELLOW = FOREGROUND_RED | FOREGROUND_GREEN |
        FOREGROUND_INTENSITY;
    const WORD COLOR_BOLD_RED = FOREGROUND_RED | FOREGROUND_INTENSITY;
    const WORD COLOR_WHITE_ON_RED = BACKGROUND_RED | FOREGROUND_RED |
        FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    
    if (console)
    {
        CONSOLE_SCREEN_BUFFER_INFO origBufferInfo;
        if (GetConsoleScreenBufferInfo(console, &origBufferInfo))
        {
            WORD attrib;
            switch (msg->level)
            {
                case LogLevelDebug: { attrib = COLOR_CYAN; } break;
                case LogLevelInfo: { attrib = COLOR_GREEN; } break;
                case LogLevelWarn: { attrib = COLOR_BOLD_YELLOW; } break;
                case LogLevelError: { attrib = COLOR_BOLD_RED; } break;
                case LogLevelFatal: { attrib = COLOR_WHITE_ON_RED;} break;
                default: { attrib = 0; } break;
            }
            
            SetConsoleTextAttribute(console, attrib);
            WriteConsoleA(console, msg->formatted,
                          (DWORD)(msg->maxSize - msg->remainingFormattingSpace),
                          0, 0);
            SetConsoleTextAttribute(console, origBufferInfo.wAttributes);
        }
    }
}
#endif

PLATFORM_WRITE_LOG_MSG(PlatformWriteLogMsg)
{
    OutputDebugStringA(msg->formatted);
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
    if (memory)
    {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
}

// TODO(yuval & eran): Renable Logging in DEBUG read & write
DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
    DEBUGReadFileResult result = { };
    
    HANDLE fileHandle = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ,
                                    0, OPEN_EXISTING, 0, 0);
    
    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER fileSize;
        if (GetFileSizeEx(fileHandle, &fileSize))
        {
            u32 fileSize32 = SafeTruncateToU32(fileSize.QuadPart);
            result.contents = VirtualAlloc(0, fileSize32,
                                           MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            
            if (result.contents)
            {
                DWORD bytesRead;
                
                if (ReadFile(fileHandle, result.contents, fileSize32, &bytesRead, 0) &&
                    (bytesRead == fileSize32))
                {
                    // NOTE(yuval): The file was read successfully
                    result.contentsSize = fileSize32;
                }
                else
                {
                    // Win32LogError("Reading File: %s Failed!", fileName);
                    DEBUGPlatformFreeFileMemory(thread, result.contents);
                    result.contents = 0;
                }
            }
            else
            {
                // Win32LogError("Allocation Faliure While Read File: %s", fileName);
            }
        }
        else
        {
            // Win32LogError("Get File Size For: %s Failed!", fileName);
        }
        
        CloseHandle(fileHandle);
    }
    else
    {
        // Win32LogError("The File: %s Does Not Exist!", fileName);
    }
    
    return result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
    b32 result = false;
    
    HANDLE fileHandle = CreateFileA(fileName,
                                    GENERIC_WRITE, 0,
                                    0,
                                    CREATE_ALWAYS,
                                    0, 0);
    
    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD bytesWritten;
        
        if (WriteFile(fileHandle, memory, memorySize, &bytesWritten, 0))
        {
            // NOTE(yuval): The file was read successfully
            result = (bytesWritten == memorySize);
        }
        else
        {
            // Win32LogError("Writing To File: %s Failed!", fileName);
        }
        
        CloseHandle(fileHandle);
    }
    else
    {
        // Win32LogError("Could Not Create Or Overwrite File: %s!", fileName);
    }
    
    return result;
}

internal void
Win32BuildEXEPathFileName(char* dest, memory_index destCount,
                          Win32State* state, const char* fileName)
{
    CatStrings(dest, destCount,
               state->exeFileName,
               state->onePastLastEXEFileNameSlash - state->exeFileName,
               fileName, StringLength(fileName));
}

internal void
Win32GetInputFileLocation(char* dest, memory_index destCount,
                          b32 isInputStream, Win32State* state,
                          s32 slotIndex)
{
    char temp[64];
    _snprintf_s(temp, sizeof(temp), "loop_edit_%d_%s.gi",
                slotIndex, isInputStream ? "input" : "state");
    
    Win32BuildEXEPathFileName(dest, destCount, state, temp);
}

internal Win32ReplayBuffer*
Win32GetReplayBuffer(Win32State* state, u32 index)
{
    Assert(index < ArrayCount(state->replayBuffers));
    Win32ReplayBuffer* result = &state->replayBuffers[index];
    return result;
}

internal void
Win32BeginRecordingInput(Win32State* state, s32 inputRecordingIndex)
{
    Win32ReplayBuffer* replayBuffer = Win32GetReplayBuffer(state, inputRecordingIndex - 1);
    
    if (replayBuffer->memoryBlock)
    {
        state->inputRecordingIndex = inputRecordingIndex;
        
        char fileName[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation(fileName, sizeof(fileName),
                                  true, state, inputRecordingIndex);
        
        state->recordingHandle = CreateFileA(fileName, GENERIC_WRITE,
                                             0, 0, CREATE_ALWAYS, 0, 0);
        
        CopyMemory(replayBuffer->memoryBlock, state->gameMemoryBlock, state->totalSize);
    }
}

internal void
Win32EndRecordingInput(Win32State* state)
{
    CloseHandle(state->recordingHandle);
    state->inputRecordingIndex = 0;
}


internal void
Win32BeginInputPlayBack(Win32State* state, s32 inputPlayingIndex)
{
    Win32ReplayBuffer* replayBuffer = Win32GetReplayBuffer(state, inputPlayingIndex - 1);
    
    if (replayBuffer->memoryBlock)
    {
        state->inputPlayingIndex = inputPlayingIndex;
        
        char fileName[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation(fileName, sizeof(fileName),
                                  true, state, inputPlayingIndex);
        
        state->playBackHandle = CreateFileA(fileName, GENERIC_READ,
                                            FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        
        CopyMemory(state->gameMemoryBlock, replayBuffer->memoryBlock, state->totalSize);
        
    }
}

internal void
Win32EndInputPlayBack(Win32State* state)
{
    CloseHandle(state->playBackHandle);
    state->inputPlayingIndex = 0;
}


internal void
Win32RecordInput(Win32State* state, GameInput* newInput)
{
    DWORD bytesWritten;
    WriteFile(state->recordingHandle, newInput,
              sizeof(*newInput), &bytesWritten, 0);
    
}

internal void
Win32PlayBackInput(Win32State* state, GameInput* newInput)
{
    DWORD bytesRead;
    ReadFile(state->playBackHandle, newInput,
             sizeof(*newInput), &bytesRead, 0);
    
    if (!bytesRead)
    {
        s32 playingIndex = state->inputPlayingIndex;
        Win32EndInputPlayBack(state);
        Win32BeginInputPlayBack(state, playingIndex);
    }
}

#if 0
internal void
DEBUGWin32DrawVertical(Win32Backbuffer* backbuffer, s32 x,
                       s32 top, s32 bottom, u32 color)
{
    if (top < 0)
    {
        top = 0;
    }
    
    if (bottom >= backbuffer->height)
    {
        bottom = backbuffer->height - 1;
    }
    
    if ((x >= 0) && (x < backbuffer->width))
    {
        u8* pixel = ((u8*)backbuffer->memory +
                     x * backbuffer->bytesPerPixel +
                     top * backbuffer->pitch);
        
        for (s32 y = top; y < bottom; ++y)
        {
            *(u32*)pixel = color;
            pixel += backbuffer->pitch;
        }
    }
}

inline void
DEBUGWin32DrawSoundBufferMarker(Win32Backbuffer* backbuffer,
                                Win32SoundOutput* soundOutput,
                                f32 c, s32 padX, s32 top, s32 bottom,
                                DWORD value, u32 color)
{
    s32 x = padX + (s32)((f32)value * c);
    DEBUGWin32DrawVertical(backbuffer, x, top, bottom, color);
}

internal void
DEBUGWin32SyncDisplay(Win32Backbuffer* backbuffer,
                      DEBUGWin32TimeMarker* markers, u32 markerCount,
                      u32 currentMarkerIndex,
                      Win32SoundOutput* soundOutput, f32 targetSecondsPerFrame)
{
    u32 playColor = 0xFFFFFFFF;
    u32 writeColor = 0xFFFF0000;
    u32 expectedFlipColor = 0xFFFFFF00;
    u32 playWindowColor = 0xFFFF00FF;
    
    s32 padX = 16;
    s32 padY = 16;
    
    s32 lineHeight = 64;
    
    f32 c = (f32)(backbuffer->width - padX * 2) / (f32)soundOutput->secondaryBufferSize;
    
    for (u32 markerIndex = 0;
         markerIndex < markerCount;
         ++markerIndex)
    {
        s32 top = padY;
        s32 bottom = padY + lineHeight;
        
        DEBUGWin32TimeMarker* thisMarker = &markers[markerIndex];
        Assert(thisMarker->outputPlayCursor < soundOutput->secondaryBufferSize);
        Assert(thisMarker->outputWriteCursor < soundOutput->secondaryBufferSize);
        Assert(thisMarker->outputLocation < soundOutput->secondaryBufferSize);
        Assert(thisMarker->outputByteCount <= soundOutput->secondaryBufferSize);
        
        Assert(thisMarker->flipPlayCursor < soundOutput->secondaryBufferSize);
        Assert(thisMarker->flipWriteCursor < soundOutput->secondaryBufferSize);
        
        if (markerIndex == currentMarkerIndex)
        {
            top += lineHeight + padY;
            bottom += lineHeight + padY;
            
            s32 firstTop = top;
            
            DEBUGWin32DrawSoundBufferMarker(backbuffer, soundOutput,
                                            c, padX, top, bottom,
                                            thisMarker->outputPlayCursor, playColor);
            
            DEBUGWin32DrawSoundBufferMarker(backbuffer, soundOutput,
                                            c, padX, top, bottom,
                                            thisMarker->outputWriteCursor, writeColor);
            
            top += lineHeight + padY;
            bottom += lineHeight + padY;
            
            DEBUGWin32DrawSoundBufferMarker(backbuffer, soundOutput,
                                            c, padX, top, bottom,
                                            thisMarker->outputLocation, playColor);
            
            DEBUGWin32DrawSoundBufferMarker(backbuffer, soundOutput,
                                            c, padX, top, bottom,
                                            thisMarker->outputLocation + thisMarker->outputByteCount,
                                            playColor);
            
            top += lineHeight + padY;
            bottom += lineHeight + padY;
            
            DEBUGWin32DrawSoundBufferMarker(backbuffer, soundOutput,
                                            c, padX, firstTop, bottom,
                                            thisMarker->expectedFlipPlayCursor, expectedFlipColor);
            
        }
        
        DEBUGWin32DrawSoundBufferMarker(backbuffer, soundOutput,
                                        c, padX, top, bottom,
                                        thisMarker->flipPlayCursor, playColor);
        
        DEBUGWin32DrawSoundBufferMarker(backbuffer, soundOutput,
                                        c, padX, top, bottom,
                                        thisMarker->flipPlayCursor + 480 * soundOutput->bytesPerSample,
                                        playWindowColor);
        
        DEBUGWin32DrawSoundBufferMarker(backbuffer, soundOutput,
                                        c, padX, top, bottom,
                                        thisMarker->flipWriteCursor, writeColor);
    }
}
#endif

internal LARGE_INTEGER
Win32GetWallClock()
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;
}

internal f32
Win32GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
    f32 result = ((f32)(end.QuadPart - start.QuadPart) /
                  (f32)globalPerfCountFrequency);
    return result;
}

internal void
Win32ProcessKeyboardMessage(GameButtonState* newState, b32 isDown)
{
    //Assert(newState->endedDown != isDown);
    newState->endedDown = isDown;
    ++newState->halfTransitionCount;
}

internal f32
Win32NormalizeXInputStick(s16 stickValue, const s16 deadZoneThreshold)
{
    f32 result = 0;
    
    if (stickValue < -deadZoneThreshold)
    {
        result = (f32)stickValue / 32768.0f;
    }
    else if (stickValue > deadZoneThreshold)
    {
        result = (f32)stickValue / 32767.0f;
    }
    
    return result;
}

internal void
Win32ProcessXInputDigitalButton(DWORD XInputButtonState,
                                DWORD buttonBit,
                                GameButtonState* oldState,
                                GameButtonState* newState)
{
    newState->endedDown = ((XInputButtonState & buttonBit) == buttonBit);
    newState->halfTransitionCount = (oldState->endedDown != newState->endedDown) ? 1 : 0;
}

internal void
Win32FillSoundBuffer(IDirectSoundBuffer* soundBuffer,
                     GameSoundOutputBuffer* sourceBuffer,
                     Win32SoundOutput* soundOutput,
                     DWORD byteToLock, DWORD bytesToWrite)
{
    void* region1;
    DWORD region1Size;
    void* region2;
    DWORD region2Size;
    // NOTE(yuval): Locking the buffer before writing
    if (SUCCEEDED(soundBuffer->Lock(byteToLock, bytesToWrite,
                                    &region1, &region1Size,
                                    &region2, &region2Size,
                                    0)))
    {
        // TODO(yuval): Assert that region1Size & region2Size are
        // an even multiply of the samples
        
        s16* sourceSample = sourceBuffer->samples;
        
        // NOTE(yuval): Writing a square wave to region 1
        u32 region1SampleCount = region1Size / soundOutput->bytesPerSample;
        s16* destSample = (s16*)region1;
        
        for (DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex)
        {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
            ++soundOutput->runningSampleIndex;
        }
        
        // NOTE(yuval): Writing a square wave to region 2
        u32 region2SampleCount = region2Size / soundOutput->bytesPerSample;
        destSample = (s16*)region2;
        for (DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex)
        {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
            ++soundOutput->runningSampleIndex;
        }
        
        globalSecondaryBuffer->Unlock(region1, region1Size,
                                      region2, region2Size);
    }
    else
    {
        // TODO(yuval & eran): Better log
        //Win32LogError("Buffer Locking Failed!");
    }
}

internal void
Win32ClearSoundBuffer(IDirectSoundBuffer* soundBuffer,
                      Win32SoundOutput* soundOutput)
{
    void* region1;
    DWORD region1Size;
    void* region2;
    DWORD region2Size;
    
    // NOTE(yuval): Locking the buffer before writing
    if (SUCCEEDED(soundBuffer->Lock(0, soundOutput->secondaryBufferSize,
                                    &region1, &region1Size,
                                    &region2, &region2Size,
                                    0)))
    {
        Assert(!region2Size);
        
        s8* destSample = (s8*)region1;
        for (DWORD byteIndex = 0; byteIndex < region1Size; ++byteIndex)
        {
            *destSample++ = 0;
        }
        
        destSample = (s8*)region2;
        for (DWORD byteIndex = 0; byteIndex < region2Size; ++byteIndex)
        {
            *destSample++ = 0;
        }
        
        globalSecondaryBuffer->Unlock(region1, region1Size,
                                      region2, region2Size);
    }
}

internal void
Win32InitDSound(HWND window, s32 samplesPerSecond, s32 bufferSize)
{
    // NOTE(yuval): Loading the direct sound library
    HMODULE DSoundLibrary = LoadLibrary("dsound.dll");
    
    // NOTE(yuval): Creating a DirectSound object
    if (DSoundLibrary)
    {
        // NOTE(yuval): Loading the DirectSoundCreate function
        DirectSoundCreateType* DirectSoundCreate = (DirectSoundCreateType*)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
        
        IDirectSound* directSound;
        
        // NOTE(yuval): Creating the DirectSound object
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &directSound, 0)))
        {
            
            WAVEFORMATEX waveFormat = { };
            waveFormat.wFormatTag = WAVE_FORMAT_PCM;
            waveFormat.nChannels = 2;
            waveFormat.nSamplesPerSec = samplesPerSecond;
            waveFormat.wBitsPerSample = 16;
            waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
            waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
            waveFormat.cbSize = 0;
            
            // NOTE(yuval): Setting the cooperative level to PRIORITY
            if (SUCCEEDED(directSound->SetCooperativeLevel(window,
                                                           DSSCL_PRIORITY)))
            {
                // NOTE(yuval): Creating the primary buffer
                DSBUFFERDESC bufferDescription = { };
                bufferDescription.dwSize = sizeof(bufferDescription);
                bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
                
                IDirectSoundBuffer* primaryBuffer;
                
                if (SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription,
                                                             &primaryBuffer, 0)))
                {
                    // NOTE(yuval): Setting the buffer format
                    if (SUCCEEDED(primaryBuffer->SetFormat(&waveFormat)))
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
            DSBUFFERDESC bufferDescription = { };
            bufferDescription.dwSize = sizeof(bufferDescription);
            bufferDescription.dwFlags = 0;
            bufferDescription.dwBufferBytes = bufferSize;
            bufferDescription.lpwfxFormat = &waveFormat;
            
            if (SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription,
                                                         &globalSecondaryBuffer, 0)))
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
        XInputGetState = (XInputGetStateType*)GetProcAddress(XInputLibrary, "XInputGetState");
        
        if (!XInputGetState)
        {
            XInputGetState = XInputGetStateStub;
        }
        
        XInputSetState = (XInputSetStateType*)GetProcAddress(XInputLibrary, "XInputSetState");
        
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

internal Win32WindowDimension
Win32GetWindowDimension(HWND window)
{
    Win32WindowDimension dim = { };
    
    RECT clientRect;
    GetClientRect(window, &clientRect);
    
    // NOTE(yuval & eran): Duplicated code, refactor into function
    dim.width = clientRect.right - clientRect.left;
    dim.height = clientRect.bottom - clientRect.top;
    
    return dim;
}

internal void
Win32ResizeDIBSection(Win32Backbuffer* buffer, s32 width, s32 height)
{
    if (buffer->memory)
    {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }
    
    buffer->width = width;
    buffer->height = height;
    buffer->bytesPerPixel = 4;
    
    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = buffer->width;
    buffer->info.bmiHeader.biHeight = -buffer->height;
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;
    
    s32 bitmapMemorySize = buffer->width * buffer->height * buffer->bytesPerPixel;
    
    buffer->memory = VirtualAlloc(0, bitmapMemorySize,
                                  MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    
    buffer->pitch = buffer->width * buffer->bytesPerPixel;
}

internal void
Win32DisplayBackbufferInWindow(HDC deviceContext, Win32Backbuffer* buffer,
                               s32 windowWidth, s32 windowHeight)
{
    StretchDIBits(deviceContext,
                  0, 0, buffer->width, buffer->height,//windowWidth, windowHeight,
                  0, 0, buffer->width, buffer->height,
                  buffer->memory,
                  &buffer->info,
                  DIB_RGB_COLORS,
                  SRCCOPY);
}

internal void
Win32ProcessPendingMessages(Win32State* state, GameController* keyboardController)
{
    MSG message;
    
    while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE))
    {
        switch (message.message)
        {
            case WM_QUIT:
            {
                globalRunning = false;
            } break;
            
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                WPARAM keyCode = message.wParam;
                bool wasDown = (message.lParam & (1 << 30)) != 0;
                bool isDown = (message.lParam & (1 << 31)) == 0;
                
                b32 altIsDown = (message.lParam & (1 << 29));
                
                if (keyCode == VK_F4 && altIsDown)
                {
                    globalRunning = false;
                }
                else if (wasDown != isDown)
                {
                    switch (keyCode)
                    {
                        case 'W':
                        {
                            Win32ProcessKeyboardMessage(&keyboardController->moveUp, isDown);
                        } break;
                        
                        case 'A':
                        {
                            Win32ProcessKeyboardMessage(&keyboardController->moveLeft, isDown);
                        } break;
                        
                        case 'S':
                        {
                            Win32ProcessKeyboardMessage(&keyboardController->moveDown, isDown);
                        } break;
                        
                        case 'D':
                        {
                            Win32ProcessKeyboardMessage(&keyboardController->moveRight, isDown);
                        } break;
                        
                        case 'Q':
                        {
                            Win32ProcessKeyboardMessage(&keyboardController->leftShoulder, isDown);
                        } break;
                        
                        case 'E':
                        {
                            Win32ProcessKeyboardMessage(&keyboardController->rightShoulder, isDown);
                        } break;
                        
                        case VK_UP:
                        {
                            Win32ProcessKeyboardMessage(&keyboardController->actionUp, isDown);
                        } break;
                        
                        case VK_DOWN:
                        {
                            Win32ProcessKeyboardMessage(&keyboardController->actionDown, isDown);
                        } break;
                        
                        case VK_LEFT:
                        {
                            Win32ProcessKeyboardMessage(&keyboardController->actionLeft, isDown);
                        } break;
                        
                        case VK_RIGHT:
                        {
                            Win32ProcessKeyboardMessage(&keyboardController->actionRight, isDown);
                        } break;
                        
                        case VK_ESCAPE:
                        {
                            Win32ProcessKeyboardMessage(&keyboardController->back, isDown);
                        } break;
                        
                        case VK_SPACE:
                        {
                            Win32ProcessKeyboardMessage(&keyboardController->start, isDown);
                        } break;
                        
#if GAME_INTERNAL
                        case 'P':
                        {
                            if (isDown)
                            {
                                globalPause = !globalPause;
                            }
                        } break;
                        
                        case 'L':
                        {
                            if (isDown)
                            {
                                if (state->inputPlayingIndex == 0)
                                {
                                    if (state->inputRecordingIndex == 0)
                                    {
                                        Win32BeginRecordingInput(state, 1);
                                    }
                                    else
                                    {
                                        Win32EndRecordingInput(state);
                                        Win32BeginInputPlayBack(state, 1);
                                    }
                                }
                                else
                                {
                                    Win32EndInputPlayBack(state);
                                    
                                    // TODO(yuval & eran): Zero the whole input
                                    // NOT JUST THE KEYBOARD CONTROLLER
                                    GameController zeroController = { };
                                    *keyboardController = zeroController;
                                }
                            }
                        }
#endif
                    }
                }
            } break;
            
            default:
            {
                TranslateMessage(&message);
                DispatchMessageA(&message);
            } break;
        }
    }
    
}
internal LRESULT CALLBACK
Win32MainWindowCallback(HWND window, UINT message,
                        WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    
    switch (message)
    {
        case WM_SIZE:
        {
        } break;
        
        case WM_DESTROY:
        {
            globalRunning = false;
        } break;
        
        case WM_CLOSE:
        {
            globalRunning = false;
        } break;
        
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            Assert(!"Keyboard Input Came In Through The Main Window Callback!");
        } break;
        
        case WM_ACTIVATEAPP:
        {
            // TODO(yuval & eran): LogDebug("Window Refocused");
        } break;
        
        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint(window, &paint);
            
            Win32WindowDimension dim = Win32GetWindowDimension(window);
            Win32DisplayBackbufferInWindow(deviceContext, &globalBackbuffer, dim.width, dim.height);
            
            EndPaint(window, &paint);
        } break;
        
        default:
        {
            result = DefWindowProc(window, message, wParam, lParam);
        } break;
    }
    
    return result;
}

internal void
Win32UnloadGameCode(Win32GameCode* gameCode)
{
    if (gameCode->gameCodeDLL)
    {
        FreeLibrary(gameCode->gameCodeDLL);
        gameCode->gameCodeDLL = 0;
    }
    
    gameCode->isValid = false;
    gameCode->UpdateAndRender = 0;
    gameCode->GetSoundSamples = 0;
    gameCode->Log = 0;
}

inline FILETIME
Win32GetLastWriteTime(const char* fileName)
{
    FILETIME lastWriteTime = { };
    
#if 0
    WIN32_FIND_DATAA findData;
    HANDLE findHandle = FindFirstFileA(fileName, &findData);
    
    if (findHandle != INVALID_HANDLE_VALUE)
    {
        lastWriteTime = findData.ftLastWriteTime;
        FindClose(findHandle);
    }
#endif
    
    WIN32_FILE_ATTRIBUTE_DATA data;
    
    if (GetFileAttributesExA(fileName, GetFileExInfoStandard, &data))
    {
        lastWriteTime = data.ftLastWriteTime;
    }
    
    return lastWriteTime;
}

internal Win32GameCode
Win32LoadGameCode(const char* sourceGameCodeDLLFullPath,
                  const char* tempGameCodeDLLFullPath)
{
    Win32GameCode result = { };
    
    result.DLLLastWriteTime = Win32GetLastWriteTime(sourceGameCodeDLLFullPath);
    
    CopyFileA(sourceGameCodeDLLFullPath, tempGameCodeDLLFullPath, FALSE);
    result.gameCodeDLL = LoadLibrary(tempGameCodeDLLFullPath);
    
    if (result.gameCodeDLL)
    {
        result.UpdateAndRender = (GameUpdateAndRenderType*)
            GetProcAddress(result.gameCodeDLL, "GameUpdateAndRender");
        result.GetSoundSamples = (GameGetSoundSamplesType*)
            GetProcAddress(result.gameCodeDLL, "GameGetSoundSamples");
        result.Log = (LogType*)
            GetProcAddress(result.gameCodeDLL, "Log");
        
        result.isValid = (result.UpdateAndRender &&
                          result.GetSoundSamples &&
                          result.Log);
    }
    
    if (!result.isValid)
    {
        result.UpdateAndRender = 0;
        result.GetSoundSamples = 0;
        result.Log = 0;
    }
    
    return result;
}

internal void
Win32GetEXEFileName(Win32State* state)
{
    DWORD sizeOfFileName = GetModuleFileNameA(0, state->exeFileName,
                                              sizeof(state->exeFileName));
    
    state->onePastLastEXEFileNameSlash = state->exeFileName + sizeOfFileName;
    
    for (char* scan = state->exeFileName; *scan; ++scan)
    {
        if (*scan == '\\' || *scan == '/')
        {
            state->onePastLastEXEFileNameSlash = scan + 1;
        }
    }
}

s32 WINAPI
WinMain(HINSTANCE instance,
        HINSTANCE prevInstance,
        LPSTR commandLine,
        s32 showCode)
{
    Win32State win32State = { };
    
    Win32GetEXEFileName(&win32State);
    
    char sourceGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(sourceGameCodeDLLFullPath,
                              sizeof(sourceGameCodeDLLFullPath),
                              &win32State, "game.dll");
    
    char tempGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(tempGameCodeDLLFullPath,
                              sizeof(tempGameCodeDLLFullPath),
                              &win32State, "game_temp.dll");
    
    LARGE_INTEGER perfCountFrequencyResult;
    QueryPerformanceFrequency(&perfCountFrequencyResult);
    globalPerfCountFrequency = perfCountFrequencyResult.QuadPart;
    
    UINT desiredSchedulerMS = 1;
    b32 sleepIsGranular = (timeBeginPeriod(desiredSchedulerMS) == TIMERR_NOERROR);
    
    Win32LoadXInput();
    
    s32 renderWidth = 960;
    s32 renderHeight = 540;
    Win32ResizeDIBSection(&globalBackbuffer, renderWidth, renderHeight);
    
    WNDCLASS windowClass = { };
    
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = Win32MainWindowCallback;
    windowClass.hInstance = instance;
    // HICON     hIcon;
    windowClass.lpszClassName = "HandmadeGameWindowClass";
    
    if (RegisterClassA(&windowClass))
    {
        HWND window = CreateWindowExA(
            0, // WS_EX_TOPMOST | WS_EX_LAYERED,
            windowClass.lpszClassName,
            "Handmade Game",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            renderWidth,
            renderHeight,
            0,
            0,
            instance,
            0);
        
        if (window)
        {
            // TODO(yuval & eran): Use this in future developer mode
            // SetLayeredWindowAttributes(window, RGB(0, 0, 0), 255, LWA_ALPHA);
            
            s32 monitorRefreshRate = 60;
            
            HDC refreshDC = GetDC(window);
            s32 win32RefreshRate = GetDeviceCaps(refreshDC, VREFRESH);
            ReleaseDC(window, refreshDC);
            
            if (win32RefreshRate > 1)
            {
                OutputDebugStringA("Windows Found A Fefresh Rate\n");
                monitorRefreshRate = win32RefreshRate;
            }
            
            char textBuffer[256];
            FormatString(textBuffer, sizeof(textBuffer),
                         "Refresh Rate:%d\n", monitorRefreshRate);
            OutputDebugStringA(textBuffer);
            
            f32 gameUpdateHz = monitorRefreshRate / 2.0f;
            f32 targetSecondsPerFrame = 1.0f / gameUpdateHz;
            
            Win32SoundOutput soundOutput = { };
            
            soundOutput.samplesPerSecond = 48000;
            soundOutput.runningSampleIndex = 0;
            soundOutput.bytesPerSample = sizeof(s16) * 2;
            soundOutput.secondaryBufferSize = soundOutput.samplesPerSecond *
                soundOutput.bytesPerSample;
            
            // TODO(yuval & eran): Find what the lowest possible safeBytes value is
            soundOutput.safetyBytes =
                (DWORD)(((f32)(soundOutput.samplesPerSecond * soundOutput.bytesPerSample) /
                         gameUpdateHz) / 3.0f);
            
            Win32InitDSound(window, soundOutput.samplesPerSecond,
                            soundOutput.secondaryBufferSize);
            
            Win32ClearSoundBuffer(globalSecondaryBuffer, &soundOutput);
            globalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
            
#if 0
            // NOTE(yuval): This tests the playCursor/writeCursor update frequency
            // on this machine (HP ProBook), it was 480 samples
            while (true)
            {
                DWORD playCursor;
                DWORD writeCursor;
                globalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor);
                
                char textBuffer[256];
                _snprintf_s(textBuffer, sizeof(textBuffer),
                            "PC:%u WC:%u\n", playCursor, writeCursor);
                OutputDebugStringA(textBuffer);
            }
#endif
            
            s16* samples = (s16*)VirtualAlloc(0, soundOutput.secondaryBufferSize,
                                              MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            
#if GAME_INTERNAL
            void* baseAddress = (void*)Terabytes(2);
#else
            void* baseAddress = 0;
#endif
            
            GameMemory gameMemory = { };
            gameMemory.permanentStorageSize = Megabytes(64);
            gameMemory.transientStorageSize = Gigabytes(1);
            
            gameMemory.PlatformGetDateTime = PlatformGetDateTime;
            gameMemory.PlatformWriteLogMsg = PlatformWriteLogMsg;
            gameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
            gameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;
            gameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
            
            win32State.totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;
            
            win32State.gameMemoryBlock = VirtualAlloc(baseAddress, win32State.totalSize,
                                                      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            
            gameMemory.permanentStorage = win32State.gameMemoryBlock;
            gameMemory.transientStorage = (u8*)gameMemory.permanentStorage +
                gameMemory.permanentStorageSize;
            
            for (s32 replayIndex = 0;
                 replayIndex < ArrayCount(win32State.replayBuffers);
                 ++replayIndex)
            {
                Win32ReplayBuffer* replayBuffer = &win32State.replayBuffers[replayIndex];
                
                Win32GetInputFileLocation(replayBuffer->replayFileName,
                                          sizeof(replayBuffer->replayFileName),
                                          false, &win32State, replayIndex + 1);
                
                replayBuffer->fileHandle = CreateFileA(replayBuffer->replayFileName, GENERIC_READ | GENERIC_WRITE,
                                                       0, 0, CREATE_ALWAYS, 0, 0);
                
                // TODO(yuval & eran): @Refactor lowhigh conversions to macros
                DWORD maxSizeHigh = (win32State.totalSize >> 32);
                DWORD maxSizeLow = (win32State.totalSize & 0xFFFFFFFF);
                
                replayBuffer->memoryMap = CreateFileMappingA(replayBuffer->fileHandle,
                                                             0,
                                                             PAGE_READWRITE,
                                                             maxSizeHigh,
                                                             maxSizeLow,
                                                             0);
                
                replayBuffer->memoryBlock = MapViewOfFile(replayBuffer->memoryMap,
                                                          FILE_MAP_ALL_ACCESS,
                                                          0, 0, win32State.totalSize);
                
                if (!replayBuffer->memoryBlock)
                {
                    // TODO(yuval & eran): @Diagnostic
                }
            }
            
            if (samples && gameMemory.permanentStorage && gameMemory.transientStorage)
            {
                GameInput inputs[2] = { };
                GameInput* newInput = &inputs[0];
                GameInput* oldInput = &inputs[1];
                
                b32 soundIsValid = false;
                DWORD audioLatencyBytes = 0;
                f32 audioLatencySeconds = 0;
                
                DEBUGWin32TimeMarker DEBUGTimeMarkers[15] = { };
                u32 DEBUGTimeMarkerIndex = 0;
                
                Win32GameCode game = Win32LoadGameCode(sourceGameCodeDLLFullPath,
                                                       tempGameCodeDLLFullPath);
                
                globalRunning = true;
                globalPause = false;
                
                LARGE_INTEGER lastCounter = Win32GetWallClock();
                LARGE_INTEGER flipWallClock = Win32GetWallClock();
                u64 lastCycleCount = __rdtsc();
                
                while (globalRunning)
                {
                    FILETIME newDLLWriteTime = Win32GetLastWriteTime(sourceGameCodeDLLFullPath);
                    
                    if (CompareFileTime(&newDLLWriteTime, &game.DLLLastWriteTime))
                    {
                        Win32UnloadGameCode(&game);
                        game = Win32LoadGameCode(sourceGameCodeDLLFullPath,
                                                 tempGameCodeDLLFullPath);
                    }
                    
                    GameController* oldKeyboardController = &oldInput->controllers[0];
                    GameController* newKeyboardController = &newInput->controllers[0];
                    
                    // TODO(yuval & eran): Zero The Struct Using A Function
                    GameController zeroController = { };
                    *newKeyboardController = zeroController;
                    newKeyboardController->isConnected = true;
                    newKeyboardController->isAnalog = false;
                    
                    for (s32 buttonIndex = 0;
                         buttonIndex < ArrayCount(newKeyboardController->buttons);
                         ++buttonIndex)
                    {
                        newKeyboardController->buttons[buttonIndex].endedDown =
                            oldKeyboardController->buttons[buttonIndex].endedDown;
                    }
                    
                    Win32ProcessPendingMessages(&win32State, newKeyboardController);
                    
                    if (!globalPause)
                    {
                        newInput->dtForFrame = targetSecondsPerFrame;
                        
                        POINT mouseP;
                        GetCursorPos(&mouseP);
                        ScreenToClient(window, &mouseP);
                        
                        newInput->mouseX = mouseP.x;
                        newInput->mouseY = mouseP.y;
                        newInput->mouseZ = 0;
                        
                        Win32ProcessKeyboardMessage(&newInput->mouseButtons[0],
                                                    GetKeyState(VK_LBUTTON) & (1 << 15));
                        
                        Win32ProcessKeyboardMessage(&newInput->mouseButtons[1],
                                                    GetKeyState(VK_MBUTTON) & (1 << 15));
                        
                        Win32ProcessKeyboardMessage(&newInput->mouseButtons[2],
                                                    GetKeyState(VK_RBUTTON) & (1 << 15));
                        
                        Win32ProcessKeyboardMessage(&newInput->mouseButtons[3],
                                                    GetKeyState(VK_XBUTTON1) & (1 << 15));
                        
                        Win32ProcessKeyboardMessage(&newInput->mouseButtons[4],
                                                    GetKeyState(VK_XBUTTON2) & (1 << 15));
                        
                        DWORD maxControllerCount = XUSER_MAX_COUNT + 1;
                        
                        if (maxControllerCount > ArrayCount(newInput->controllers))
                        {
                            maxControllerCount = ArrayCount(newInput->controllers);
                        }
                        
                        for (DWORD controllerIndex = 1;
                             controllerIndex < maxControllerCount;
                             ++controllerIndex)
                        {
                            XINPUT_STATE controllerState;
                            
                            GameController* oldController = &oldInput->controllers[controllerIndex];
                            GameController* newController = &newInput->controllers[controllerIndex];
                            
                            if (XInputGetState(controllerIndex - 1, &controllerState) == ERROR_SUCCESS)
                            {
                                newController->isConnected = true;
                                newController->isAnalog = oldController->isAnalog;
                                
                                XINPUT_GAMEPAD* pad = &controllerState.Gamepad;
                                
                                // NOTE(yuval): Stick Processing
                                newController->stickAverageX =
                                    Win32NormalizeXInputStick(pad->sThumbLX,
                                                              XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                
                                
                                newController->stickAverageY =
                                    Win32NormalizeXInputStick(pad->sThumbLY,
                                                              XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                
                                f32 threshold = 0.5f;
                                
                                // NOTE(yuval): Stick X Fake Digital Button Processing
                                Win32ProcessXInputDigitalButton(
                                    (newController->stickAverageX < -threshold) ? 1 : 0,
                                    1,
                                    &oldController->moveLeft,
                                    &newController->moveLeft);
                                
                                Win32ProcessXInputDigitalButton(
                                    (newController->stickAverageX > threshold) ? 1 : 0,
                                    1,
                                    &oldController->moveRight,
                                    &newController->moveRight);
                                
                                // NOTE(yuval): Stick Y Fake Digital Button Processing
                                Win32ProcessXInputDigitalButton(
                                    (newController->stickAverageY < -threshold) ? 1 : 0,
                                    1,
                                    &oldController->moveDown,
                                    &newController->moveDown);
                                
                                Win32ProcessXInputDigitalButton(
                                    (newController->stickAverageY > threshold) ? 1 : 0,
                                    1,
                                    &oldController->moveUp,
                                    &newController->moveUp);
                                
                                // NOTE(yuval): DPAD Processing
                                // NOTE(yuval): DPAD Processing as fake stick
                                if (newController->stickAverageX != 0 ||
                                    newController->stickAverageY != 0)
                                {
                                    newController->isAnalog = true;
                                }
                                
                                if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                                {
                                    newController->stickAverageY = 1.0f;
                                    newController->isAnalog = false;
                                }
                                
                                if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                                {
                                    newController->stickAverageY = -1.0f;
                                    newController->isAnalog = false;
                                }
                                
                                if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                                {
                                    newController->stickAverageX = -1.0f;
                                    newController->isAnalog = false;
                                }
                                
                                if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                                {
                                    newController->stickAverageX = 1.0f;
                                    newController->isAnalog = false;
                                }
                                
                                // NOTE(yuval): DPAD Processing as digital button
                                Win32ProcessXInputDigitalButton(pad->wButtons,
                                                                XINPUT_GAMEPAD_DPAD_UP,
                                                                &oldController->moveUp,
                                                                &newController->moveUp);
                                
                                Win32ProcessXInputDigitalButton(pad->wButtons,
                                                                XINPUT_GAMEPAD_DPAD_DOWN,
                                                                &oldController->moveDown,
                                                                &newController->moveDown);
                                
                                Win32ProcessXInputDigitalButton(pad->wButtons,
                                                                XINPUT_GAMEPAD_DPAD_LEFT,
                                                                &oldController->moveLeft,
                                                                &newController->moveLeft);
                                
                                Win32ProcessXInputDigitalButton(pad->wButtons,
                                                                XINPUT_GAMEPAD_DPAD_RIGHT,
                                                                &oldController->moveRight,
                                                                &newController->moveRight);
                                
                                // NOTE(yuval): Digital Button Processing
                                Win32ProcessXInputDigitalButton(pad->wButtons,
                                                                XINPUT_GAMEPAD_Y,
                                                                &oldController->actionUp,
                                                                &newController->actionUp);
                                
                                Win32ProcessXInputDigitalButton(pad->wButtons,
                                                                XINPUT_GAMEPAD_Y,
                                                                &oldController->actionUp,
                                                                &newController->actionUp);
                                
                                Win32ProcessXInputDigitalButton(pad->wButtons,
                                                                XINPUT_GAMEPAD_A,
                                                                &oldController->actionDown,
                                                                &newController->actionDown);
                                
                                Win32ProcessXInputDigitalButton(pad->wButtons,
                                                                XINPUT_GAMEPAD_X,
                                                                &oldController->actionLeft,
                                                                &newController->actionLeft);
                                
                                Win32ProcessXInputDigitalButton(pad->wButtons,
                                                                XINPUT_GAMEPAD_B,
                                                                &oldController->actionRight,
                                                                &newController->actionRight);
                                
                                Win32ProcessXInputDigitalButton(pad->wButtons,
                                                                XINPUT_GAMEPAD_LEFT_SHOULDER,
                                                                &oldController->leftShoulder,
                                                                &newController->leftShoulder);
                                
                                Win32ProcessXInputDigitalButton(pad->wButtons,
                                                                XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                                                &oldController->rightShoulder,
                                                                &newController->rightShoulder);
                                
                                Win32ProcessXInputDigitalButton(pad->wButtons,
                                                                XINPUT_GAMEPAD_START,
                                                                &oldController->start,
                                                                &newController->start);
                                
                                Win32ProcessXInputDigitalButton(pad->wButtons,
                                                                XINPUT_GAMEPAD_BACK,
                                                                &oldController->back,
                                                                &newController->back);
                            }
                            else
                            {
                                newController->isConnected = false;
                            }
                        }
                        
                        ThreadContext thread = { };
                        
                        GameOffscreenBuffer offscreenBuffer = { };
                        offscreenBuffer.memory = globalBackbuffer.memory;
                        offscreenBuffer.width = globalBackbuffer.width;
                        offscreenBuffer.height = globalBackbuffer.height;
                        offscreenBuffer.pitch = globalBackbuffer.pitch;
                        offscreenBuffer.bytesPerPixel = globalBackbuffer.bytesPerPixel;
                        
                        if (win32State.inputRecordingIndex)
                        {
                            Win32RecordInput(&win32State, newInput);
                        }
                        
                        if (win32State.inputPlayingIndex)
                        {
                            Win32PlayBackInput(&win32State, newInput);
                        }
                        
                        if (game.UpdateAndRender)
                        {
                            game.UpdateAndRender(&thread, &gameMemory, newInput, &offscreenBuffer);
                        }
                        
                        LARGE_INTEGER audioCounter = Win32GetWallClock();
                        f32 fromBeginToAudioSeconds = Win32GetSecondsElapsed(flipWallClock, audioCounter);
                        
                        DWORD playCursor;
                        DWORD writeCursor;
                        if (globalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor) == DS_OK)
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
                            
                            if (!soundIsValid)
                            {
                                soundOutput.runningSampleIndex = writeCursor / soundOutput.bytesPerSample;
                                soundIsValid = true;
                            }
                            
                            DWORD byteToLock =
                                (soundOutput.runningSampleIndex * soundOutput.bytesPerSample)
                                % soundOutput.secondaryBufferSize;
                            
                            DWORD expectedSoundBytesPerFrame =
                                (DWORD)((soundOutput.samplesPerSecond * soundOutput.bytesPerSample) /
                                        gameUpdateHz);
                            
                            f32 secondsLeftUntilFlip = (targetSecondsPerFrame - fromBeginToAudioSeconds);
                            
                            DWORD expectedBytesUntilFlip =
                                (DWORD)((secondsLeftUntilFlip / targetSecondsPerFrame) *
                                        expectedSoundBytesPerFrame);
                            
                            DWORD expectedFrameBoundaryByte = (playCursor + expectedBytesUntilFlip);
                            
                            DWORD safeWriteCursor = writeCursor;
                            if (safeWriteCursor < playCursor)
                            {
                                safeWriteCursor += soundOutput.secondaryBufferSize;
                            }
                            Assert(safeWriteCursor >= playCursor);
                            safeWriteCursor += soundOutput.safetyBytes;
                            
                            b32 audioCardIsLowLatency = (safeWriteCursor < expectedFrameBoundaryByte);
                            
                            DWORD targetCursor = 0;
                            if (audioCardIsLowLatency)
                            {
                                targetCursor = (expectedFrameBoundaryByte + expectedSoundBytesPerFrame);
                            }
                            else
                            {
                                targetCursor = (writeCursor + expectedSoundBytesPerFrame +
                                                soundOutput.safetyBytes);
                            }
                            targetCursor %= soundOutput.secondaryBufferSize;
                            
                            DWORD bytesToWrite = 0;
                            if (byteToLock > targetCursor)
                            {
                                bytesToWrite = soundOutput.secondaryBufferSize - byteToLock;
                                bytesToWrite += targetCursor;
                            }
                            else
                            {
                                bytesToWrite = targetCursor - byteToLock;
                            }
                            
                            GameSoundOutputBuffer soundBuffer = { };
                            soundBuffer.samplesPerSecond = soundOutput.samplesPerSecond;
                            soundBuffer.sampleCount = bytesToWrite / soundOutput.bytesPerSample;
                            soundBuffer.samples = samples;
                            
                            if (game.GetSoundSamples)
                            {
                                game.GetSoundSamples(&thread, &gameMemory, &soundBuffer);
                            }
                            
#if GAME_INTERNAL
                            DEBUGWin32TimeMarker* marker = &DEBUGTimeMarkers[DEBUGTimeMarkerIndex];
                            marker->outputPlayCursor = playCursor;
                            marker->outputWriteCursor = writeCursor;
                            marker->outputLocation = byteToLock;
                            marker->outputByteCount = bytesToWrite;
                            marker->expectedFlipPlayCursor = expectedFrameBoundaryByte;
#endif
                            
                            Win32FillSoundBuffer(globalSecondaryBuffer,
                                                 &soundBuffer, &soundOutput,
                                                 byteToLock, bytesToWrite);
                            
                            DWORD unwrappedWriteCursor = writeCursor;
                            if (writeCursor < playCursor)
                            {
                                unwrappedWriteCursor += soundOutput.secondaryBufferSize;
                            }
                            
                            audioLatencyBytes = unwrappedWriteCursor - playCursor;
                            
                            audioLatencySeconds =
                                (((f32)audioLatencyBytes / (f32)soundOutput.bytesPerSample) /
                                 (f32)soundOutput.samplesPerSecond);
                            
#if 0 //GAME_INTERNAL
                            Win32LogDebug("BTL:%u, TC:%u, BTW:%u - PC:%u WC:%u DELTA:%u (%fs)",
                                          byteToLock, targetCursor,
                                          bytesToWrite, playCursor,
                                          writeCursor,
                                          audioLatencyBytes,
                                          audioLatencySeconds);
#endif
                        }
                        else
                        {
                            soundIsValid = false;
                        }
                        
                        LARGE_INTEGER workCounter = Win32GetWallClock();
                        f32 workSecondsElapsed = Win32GetSecondsElapsed(lastCounter, workCounter);
                        
                        f32 secondsElapsedForFrame = workSecondsElapsed;
                        if (secondsElapsedForFrame < targetSecondsPerFrame)
                        {
                            if (sleepIsGranular)
                            {
                                DWORD sleepMS = (DWORD)(1000.0f * (targetSecondsPerFrame - secondsElapsedForFrame));
                                
                                if (sleepMS > 0)
                                {
                                    Sleep(sleepMS);
                                }
                                
                                secondsElapsedForFrame = Win32GetSecondsElapsed(lastCounter,
                                                                                Win32GetWallClock());
                            }
                            
                            if (secondsElapsedForFrame > targetSecondsPerFrame)
                            {
                                Win32LogError("Sleep Missed Frame Rate Of %.2f\n", gameUpdateHz);
                            }
                            
                            while (secondsElapsedForFrame < targetSecondsPerFrame)
                            {
                                secondsElapsedForFrame = Win32GetSecondsElapsed(lastCounter,
                                                                                Win32GetWallClock());
                            }
                        }
                        else
                        {
                            // TODO(eran & yuval): MISSED FRAME RATE!
                            Win32LogError("Frame Rate Of %.2f Missed", gameUpdateHz);
                        }
                        
                        flipWallClock = Win32GetWallClock();
                        
#if 0 //GAME_INTERNAL
                        DEBUGWin32SyncDisplay(&globalBackbuffer,
                                              DEBUGTimeMarkers, ArrayCount(DEBUGTimeMarkers),
                                              DEBUGTimeMarkerIndex - 1,
                                              &soundOutput, targetSecondsPerFrame);
#endif
                        
                        Win32WindowDimension dim = Win32GetWindowDimension(window);
                        HDC deviceContext = GetDC(window);
                        Win32DisplayBackbufferInWindow(deviceContext, &globalBackbuffer, dim.width, dim.height);
                        ReleaseDC(window, deviceContext);
                        
                        // NOTE(yuval): This is debug code
#if GAME_INTERNAL
                        {
                            DWORD DEBUGPlayCursor;
                            DWORD DEBUGWriteCursor;
                            if (globalSecondaryBuffer->GetCurrentPosition(&DEBUGPlayCursor,
                                                                          &DEBUGWriteCursor) == DS_OK)
                            {
                                Assert(DEBUGTimeMarkerIndex < ArrayCount(DEBUGTimeMarkers));
                                
                                DEBUGWin32TimeMarker* marker = &DEBUGTimeMarkers[DEBUGTimeMarkerIndex];
                                
                                marker->flipPlayCursor = DEBUGPlayCursor;
                                marker->flipWriteCursor = DEBUGWriteCursor;
                            }
                            
                            ++DEBUGTimeMarkerIndex;
                            
                            if (DEBUGTimeMarkerIndex == ArrayCount(DEBUGTimeMarkers))
                            {
                                DEBUGTimeMarkerIndex = 0;
                            }
                            
                        }
#endif
                        
                        f32 msPerFrame = (1000.0f * Win32GetSecondsElapsed(lastCounter, flipWallClock));
                        f32 fps = ((f32)globalPerfCountFrequency /
                                   (f32)(flipWallClock.QuadPart - lastCounter.QuadPart));
                        
                        Win32LogDebug("%.2fms/f,  %.2ff/s", msPerFrame, fps);
#if 0
                        u64 endCycleCount = __rdtsc();
                        
                        u64 cyclesElapsed = endCycleCount - lastCycleCount;
                        
                        f32 msPerFrame = (1000.0f * (f32)counterElapsed) / (f32)perfCountFrequency;
                        f32 fps = (f32)perfCountFrequency / (f32)counterElapsed;
                        f32 mcpf = (f32)cyclesElapsed / 1000000.0f;
                        
                        Win32LogDebug("%.2fms/f,  %.2f/s,  %.2fmc/f", msPerFrame, fps, mcpf);
                        
                        lastCycleCount = endCycleCount;
#endif
                        
                        // TODO(yuval & eran): Metaprogramming SWAP
                        GameInput* temp = newInput;
                        newInput = oldInput;
                        oldInput = temp;
                        
                        lastCounter = flipWallClock;
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
