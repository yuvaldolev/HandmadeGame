#include "game.cpp"

#include <windows.h>
#include <dsound.h>
#include <xinput.h>

#include <math.h>
#include <stdio.h> // TODO(yuvai & eran): Remove this temporary include
#include <malloc.h>

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
global_variable bool globalRunning;
global_variable Win32Backbuffer globalBackbuffer;
global_variable IDirectSoundBuffer* globalSeconderyBuffer;
global_variable s64 globalPerfCountFrequency;

// TODO(yuval & eran): Temporary!
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

PlatformDateTime
PlatformGetDateTime()
{
    SYSTEMTIME localTime;
    GetLocalTime(&localTime);
    
    PlatformDateTime dateTime;
    dateTime.day = localTime.wDay;
    dateTime.month = localTime.wMonth;
    dateTime.year = localTime.wYear;
    dateTime.hour = localTime.wHour;
    dateTime.minute = localTime.wMinute;
    dateTime.second = localTime.wSecond;
    dateTime.milliseconds = localTime.wMilliseconds;
    
    return dateTime;
}

DEBUGReadFileResult
DEBUGPlatformReadEntireFile(const char* filename)
{
    DEBUGReadFileResult result = { };
    
    HANDLE fileHandle = CreateFileA(filename,
                                    GENERIC_READ, FILE_SHARE_READ,
                                    0,
                                    OPEN_EXISTING,
                                    0, 0);
    
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
                    LogError("Reading File: %s Failed!", filename);
                    DEBUGPlatformFreeFileMemory(result.contents);
                    result.contents = 0;
                }
            }
            else
            {
                LogError("Allocation Faliure While Read File: %s", filename);
            }
        }
        else
        {
            LogError("Get File Size For: %s Failed!", filename);
        }
        
        CloseHandle(fileHandle);
    }
    else
    {
        LogError("The File: %s Does Not Exist!", filename);
    }
    
    return result;
}


b32
DEBUGPlatformWriteEntireFile(const char* filename,
                             void* memory, u32 memorySize)
{
    b32 result = false;
    
    HANDLE fileHandle = CreateFileA(filename,
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
            LogError("Writing To File: %s Failed!", filename);
        }
        
        CloseHandle(fileHandle);
    }
    else
    {
        LogError("Could Not Create Or Overwrite File: %s!", filename);
    }
    
    return result;
}

void
DEBUGPlatformFreeFileMemory(void* memory)
{
    if (memory)
    {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
}

internal void
Win32DEBUGDisplayVertical(Win32Backbuffer* backbuffer, s32 x,
                          s32 top, s32 bottom, u32 color)
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

internal void
Win32DEBUGSyncDisplay(Win32Backbuffer* backbuffer,
                      DWORD* playCursors, u32 playCursorsCount,
                      Win32SoundOutput* soundOutput, r32 targetSecondsPerFrame)
{
    s32 padX = 16;
    s32 padY = 16;
    
    s32 top = padY;
    s32 bottom = backbuffer->height - padY;
    
    r32 c = (r32)(backbuffer->width - padX * 2) / (r32)soundOutput->seconderyBufferSize;
    
    for (u32 playCursorIndex = 0;
         playCursorIndex < playCursorsCount;
         ++playCursorIndex)
    {
        s32 x = padX + (s32)(playCursors[playCursorIndex] * c);
        Win32DEBUGDisplayVertical(backbuffer, x, top, bottom, 0xFFFFFFFF);
    }
}

internal LARGE_INTEGER
Win32GetWallClock()
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;
}

internal r32
Win32GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
    r32 result = (r32(end.QuadPart - start.QuadPart) /
                  (r32)globalPerfCountFrequency);
    return result;
}

internal void
Win32ProcessKeyboardMessage(GameButtonState* newState, b32 isDown)
{
    Assert(newState->endedDown != isDown);
    newState->endedDown = isDown;
    ++newState->halfTransitionCount;
}

internal r32
Win32NormalizeXInputStick(s16 stickValue, const s16 deadZoneThreshold)
{
    r32 result = 0;
    
    if (stickValue < -deadZoneThreshold)
    {
        result = (r32)stickValue / 32768.0f;
    }
    else if (stickValue > deadZoneThreshold)
    {
        result = (r32)stickValue / 32767.0f;
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
        
        globalSeconderyBuffer->Unlock(region1, region1Size,
                                      region2, region2Size);
    }
    else
    {
        // TODO(yuval & eran): Better log
        //LogError("Buffer Locking Failed!");
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
    if (SUCCEEDED(soundBuffer->Lock(0, soundOutput->seconderyBufferSize,
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
        
        globalSeconderyBuffer->Unlock(region1, region1Size,
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
                                                         &globalSeconderyBuffer, 0)))
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

// TODO(yuval & eran): This is temporary
internal void
Win32OpenConsole()
{
    AllocConsole();
    FILE* ignored;
    freopen_s(&ignored, "CONOUT$", "w", stdout);
    freopen_s(&ignored, "CONOUT$", "w", stderr);
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
                  0, 0, windowWidth, windowHeight,
                  0, 0, buffer->width, buffer->height,
                  buffer->memory,
                  &buffer->info,
                  DIB_RGB_COLORS,
                  SRCCOPY);
}

internal void
Win32ProcessPendingMessages(GameController* keyboardController)
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

s32 WINAPI
WinMain(HINSTANCE instance,
        HINSTANCE prevInstance,
        LPSTR commandLine,
        s32 showCode)
{
    LARGE_INTEGER perfCountFrequencyResult;
    QueryPerformanceFrequency(&perfCountFrequencyResult);
    globalPerfCountFrequency = perfCountFrequencyResult.QuadPart;
    
    UINT desiredSchedulerMS = 1;
    b32 sleepIsGranular = (timeBeginPeriod(desiredSchedulerMS) == TIMERR_NOERROR);
    
    // TODO(yuval & eran): This is temporary
    Win32OpenConsole();
    
    Win32LoadXInput();
    
    Win32ResizeDIBSection(&globalBackbuffer, 1280, 720);
    
    WNDCLASS windowClass = { };
    
    windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = Win32MainWindowCallback;
    windowClass.hInstance = instance;
    // HICON     hIcon;
    windowClass.lpszClassName = "HandmadeGameWindowClass";
    
#define monitorRefreshRate 60
#define gameUpdateHz (monitorRefreshRate / 2)
#define targetSecondsPerFrame (1.0f / gameUpdateHz)
    
    if (RegisterClassA(&windowClass))
    {
        HWND window = CreateWindowExA(
            0,
            windowClass.lpszClassName,
            "Handmade Game",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            instance,
            0);
        
        if (window)
        {
            HDC deviceContext = GetDC(window);
            
            Win32SoundOutput soundOutput = { };
            
            soundOutput.samplesPerSecond = 48000;
            soundOutput.runningSampleIndex = 0;
            soundOutput.bytesPerSample = sizeof(s16) * 2;
            soundOutput.seconderyBufferSize = soundOutput.samplesPerSecond *
                soundOutput.bytesPerSample;
            soundOutput.latencySampleCount = soundOutput.samplesPerSecond / 15;
            
            Win32InitDSound(window, soundOutput.samplesPerSecond,
                            soundOutput.seconderyBufferSize);
            
            Win32ClearSoundBuffer(globalSeconderyBuffer, &soundOutput);
            globalSeconderyBuffer->Play(0, 0, DSBPLAY_LOOPING);
            
            s16* samples = (s16*)VirtualAlloc(0, soundOutput.seconderyBufferSize,
                                              MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            
#if GAME_INTERNAL
            void* baseAddress = (void*)Terabytes(2);
#else
            void* baseAddress = 0;
#endif
            
            GameMemory gameMemory = { };
            gameMemory.permanentStorageSize = Megabytes(64);
            gameMemory.transientStorageSize = Gigabytes(1);
            
            u64 totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;
            
            gameMemory.permanentStorage = VirtualAlloc(baseAddress, totalSize,
                                                       MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            gameMemory.transientStorage = (u8*)gameMemory.permanentStorage +
                gameMemory.permanentStorageSize;
            
            if (samples && gameMemory.permanentStorage && gameMemory.transientStorage)
            {
                GameInput inputs[2] = { };
                GameInput* newInput = &inputs[0];
                GameInput* oldInput = &inputs[1];
                
                globalRunning = true;
                
                u64 lastCycleCount = __rdtsc();
                
                LARGE_INTEGER lastCounter = Win32GetWallClock();
                
                DWORD DEBUGPlayCursors[gameUpdateHz / 2] = { };
                u32 DEBUGLastPlayCursorIndex = 0;
                
                while (globalRunning)
                {
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
                    
                    Win32ProcessPendingMessages(newKeyboardController);
                    
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
                            
                            XINPUT_GAMEPAD* pad = &controllerState.Gamepad;
                            
                            // NOTE(yuval): Stick Processing
                            newController->stickAverageX =
                                Win32NormalizeXInputStick(pad->sThumbLX,
                                                          XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                            
                            
                            newController->stickAverageY =
                                Win32NormalizeXInputStick(pad->sThumbLY,
                                                          XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                            
                            r32 threshold = 0.5f;
                            
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
                            newController->isAnalog = true;
                            
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
                    
                    b32 soundIsValid = false;
                    
                    DWORD playCursor = 0;
                    DWORD writeCursor = 0;
                    DWORD targetCursor = 0;
                    DWORD byteToLock = 0;
                    DWORD bytesToWrite = 0;
                    
                    if (SUCCEEDED(globalSeconderyBuffer->
                                  GetCurrentPosition(&playCursor, &writeCursor)))
                    {
                        soundIsValid = true;
                        
                        byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample)
                            % soundOutput.seconderyBufferSize;
                        
                        targetCursor = ((playCursor +
                                         (soundOutput.latencySampleCount * soundOutput.bytesPerSample)) %
                                        soundOutput.seconderyBufferSize);
                        
                        if (byteToLock > targetCursor)
                        {
                            bytesToWrite = soundOutput.seconderyBufferSize - byteToLock;
                            bytesToWrite += targetCursor;
                        }
                        else
                        {
                            bytesToWrite = targetCursor - byteToLock;
                        }
                    }
                    
                    GameOffscreenBuffer offscreenBuffer = { };
                    offscreenBuffer.memory = globalBackbuffer.memory;
                    offscreenBuffer.width = globalBackbuffer.width;
                    offscreenBuffer.height = globalBackbuffer.height;
                    offscreenBuffer.pitch = globalBackbuffer.pitch;
                    
                    GameSoundOutputBuffer soundBuffer = { };
                    soundBuffer.samplesPerSecond = soundOutput.samplesPerSecond;
                    soundBuffer.sampleCount = bytesToWrite / soundOutput.bytesPerSample;
                    soundBuffer.samples = samples;
                    
                    GameUpdateAndRender(&gameMemory, newInput, &offscreenBuffer, &soundBuffer);
                    
                    if (soundIsValid)
                    {
                        Win32FillSoundBuffer(globalSeconderyBuffer,
                                             &soundBuffer, &soundOutput,
                                             byteToLock, bytesToWrite);
                    }
                    
                    LARGE_INTEGER workCounter = Win32GetWallClock();
                    r32 workSecondsElapsed = Win32GetSecondsElapsed(lastCounter, workCounter);
                    
                    r32 secondsElapsedForFrame = workSecondsElapsed;
                    if (secondsElapsedForFrame <= targetSecondsPerFrame)
                    {
                        while (secondsElapsedForFrame < targetSecondsPerFrame)
                        {
                            if (sleepIsGranular)
                            {
                                DWORD sleepMS = (DWORD)(1000.0f * (targetSecondsPerFrame - secondsElapsedForFrame));
                                Sleep(sleepMS);
                            }
                            
                            secondsElapsedForFrame = Win32GetSecondsElapsed(lastCounter,
                                                                            Win32GetWallClock());
                        }
                    }
                    else
                    {
                        // TODO(eran & yuval): MISSED FRAME RATE!
                        LogInfo("Frame Rate Of %d Missed", gameUpdateHz);
                    }
                    
                    LARGE_INTEGER endCounter = Win32GetWallClock();
                    
#if GAME_INTERNAL
                    Win32DEBUGSyncDisplay(&globalBackbuffer,
                                          DEBUGPlayCursors, ArrayCount(DEBUGPlayCursors),
                                          &soundOutput, targetSecondsPerFrame);
#endif
                    
                    Win32WindowDimension dim = Win32GetWindowDimension(window);
                    Win32DisplayBackbufferInWindow(deviceContext, &globalBackbuffer, dim.width, dim.height);
                    
                    // NOTE(yuval): This is debug code
#if GAME_INTERNAL
                    {
                        Assert(DEBUGLastPlayCursorIndex < ArrayCount(DEBUGPlayCursors));
                        
                        DWORD lastPlayCursor;
                        globalSeconderyBuffer->GetCurrentPosition(&lastPlayCursor, 0);
                        
                        DEBUGPlayCursors[DEBUGLastPlayCursorIndex++] = lastPlayCursor;
                        
                        if (DEBUGLastPlayCursorIndex == ArrayCount(DEBUGPlayCursors))
                        {
                            DEBUGLastPlayCursorIndex = 0;
                        }
                    }
#endif
                    
                    r32 msPerFrame = (1000.0f * Win32GetSecondsElapsed(lastCounter, endCounter));
                    r32 fps = ((r32)globalPerfCountFrequency /
                               (r32)(endCounter.QuadPart - lastCounter.QuadPart));
                    
                    LogDebug("%.2fms/f,  %.2ff/s", msPerFrame, fps);
#if 0
                    u64 endCycleCount = __rdtsc();
                    
                    u64 cyclesElapsed = endCycleCount - lastCycleCount;
                    
                    r32 msPerFrame = (1000.0f * (r32)counterElapsed) / (r32)perfCountFrequency;
                    r32 fps = (r32)perfCountFrequency / (r32)counterElapsed;
                    r32 mcpf = (r32)cyclesElapsed / 1000000.0f;
                    
                    LogDebug("%.2fms/f,  %.2f/s,  %.2fmc/f", msPerFrame, fps, mcpf);
                    
                    lastCycleCount = endCycleCount;
#endif
                    
                    // TODO(yuval & eran): Metaprogramming SWAP
                    GameInput* temp = newInput;
                    newInput = oldInput;
                    oldInput = temp;
                    
                    lastCounter = endCounter;
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

