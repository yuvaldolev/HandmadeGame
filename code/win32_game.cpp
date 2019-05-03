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

internal r32
Win32NormalizeXInputStick(s16 stickValue)
{
    r32 result;
    
    if (stickValue < 0)
    {
        result = (r32)stickValue / 32768.0f;
    }
    else
    {
        result = (r32)stickValue / 32767.0f;
    }
    
    return result;
}

internal void
Win32ProcessXInputButton(DWORD XInputButtonState,
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
        // TODO(yuval & eran): Assert that region2Size is 0
        
        s8* destSample = (s8*)region1;
        for (DWORD sampleIndex = 0; sampleIndex < region1Size; ++sampleIndex)
        {
            *destSample++ = 0;
        }
        
        destSample = (s8*)region2;
        for (DWORD sampleIndex = 0; sampleIndex < region2Size; ++sampleIndex)
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
                        LogInfo("Primary buffer format was set");
                    }
                    else
                    {
                        LogError("Primary Buffer Formatting Failed!");
                    }
                }
            }
            else
            {
                LogError("DirectSound Cooperatrive Level Setting Failed!");
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
                LogInfo("The secondery sound buffer was created");
            }
        }
        else
        {
            LogError("DirectSound Object Creation Failed!");
        }
    }
    else
    {
        LogError("dsound.dll Loading Failed!");
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
        LogError("Failed To Load xinput1_4.dll and xinput1_3.dll!");
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
            WPARAM keyCode = wParam;
            bool wasDown = (lParam & (1 << 30)) != 0;
            bool isDown = (lParam & (1 << 31)) == 0;
            
            b32 altIsDown = lParam & (1 << 29);
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
                    } break;
                    
                    case 'A':
                    {
                    } break;
                    
                    case 'S':
                    {
                    } break;
                    
                    case 'D':
                    {
                    } break;
                    
                    case 'Q':
                    {
                        
                    } break;
                    
                    case 'E':
                    {
                        
                    } break;
                    
                    case VK_UP:
                    {
                        
                    } break;
                    
                    case VK_DOWN:
                    {
                        
                    } break;
                    
                    case VK_LEFT:
                    {
                        
                    } break;
                    
                    case VK_RIGHT:
                    {
                        
                    } break;
                    
                    case VK_ESCAPE:
                    {
                        
                    } break;
                    
                    case VK_SPACE:
                    {
                    } break;
                }
            }
        } break;
        
        case WM_ACTIVATEAPP:
        {
            LogDebug("Window Refocused");
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
    s64 perfCountFrequency = perfCountFrequencyResult.QuadPart;
    
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
            
            Win32InitDSound(window, soundOutput.samplesPerSecond,
                            soundOutput.seconderyBufferSize);
            
            Win32ClearSoundBuffer(globalSeconderyBuffer, &soundOutput);
            globalSeconderyBuffer->Play(0, 0, DSBPLAY_LOOPING);
            
            s16* samples = (s16*)VirtualAlloc(0, soundOutput.seconderyBufferSize,
                                              MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            
#if GAME_INTERNAL
            void* baseAddress = (void*)Terabytes((u64)2);
#else
            void* baseAddress = 0;
#endif
            
            GameMemory gameMemory = { };
            gameMemory.permanentStorageSize = Megabytes(64);
            gameMemory.transientStorageSize = Gigabytes((u64)1);
            
            u64 totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;
            
            gameMemory.permanentStorage = VirtualAlloc(baseAddress, totalSize,
                                                       MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            gameMemory.transientStorage = (u8*)gameMemory.permanentStorage +
                gameMemory.permanentStorageSize;
            
            GameInput inputs[2] = { };
            GameInput* newInput = &inputs[0];
            GameInput* oldInput = &inputs[1];
            
            globalRunning = true;
#if 0
            u64 lastCycleCount = __rdtsc();
            
            LARGE_INTEGER lastCounter;
            QueryPerformanceCounter(&lastCounter);
#endif
            
            while (globalRunning)
            {
                MSG message;
                
                while (PeekMessageA(&message, window, 0, 0, PM_REMOVE))
                {
                    if (message.message == WM_QUIT)
                    {
                        globalRunning = false;
                    }
                    
                    TranslateMessage(&message);
                    DispatchMessageA(&message);
                }
                
                DWORD maxControllerCount = XUSER_MAX_COUNT;
                
                if (maxControllerCount > ArrayCount(newInput->controllers))
                {
                    maxControllerCount = ArrayCount(newInput->controllers);
                }
                
                for (DWORD controllerIndex = 0;
                     controllerIndex < maxControllerCount;
                     ++controllerIndex)
                {
                    XINPUT_STATE controllerState;
                    
                    if (XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS)
                    {
                        GameController* oldController = &oldInput->controllers[controllerIndex];
                        GameController* newController = &newInput->controllers[controllerIndex];
                        
                        newController->isAnalog = true;
                        
                        XINPUT_GAMEPAD* pad = &controllerState.Gamepad;
                        
                        b32 up = pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        b32 down = pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                        b32 left = pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        b32 right = pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                        
                        /*b32 start = pad->wButtons & XINPUT_GAMEPAD_START;
                        b32 back = pad->wButtons & XINPUT_GAMEPAD_BACK;*/
                        
                        // TODO(yuval & eran): Do this only in first pole each frame
                        newController->startX = oldController->startX;
                        newController->startY = oldController->startY;
                        
                        newController->endX = newController->minX = newController->maxX =
                            Win32NormalizeXInputStick(pad->sThumbLX);
                        
                        newController->endY = newController->minY = newController->maxY =
                            Win32NormalizeXInputStick(pad->sThumbLY);
                        
                        Win32ProcessXInputButton(pad->wButtons,
                                                 XINPUT_GAMEPAD_Y,
                                                 &oldController->up,
                                                 &newController->up);
                        
                        Win32ProcessXInputButton(pad->wButtons,
                                                 XINPUT_GAMEPAD_A,
                                                 &oldController->down,
                                                 &newController->down);
                        
                        Win32ProcessXInputButton(pad->wButtons,
                                                 XINPUT_GAMEPAD_X,
                                                 &oldController->left,
                                                 &newController->left);
                        
                        Win32ProcessXInputButton(pad->wButtons,
                                                 XINPUT_GAMEPAD_B,
                                                 &oldController->right,
                                                 &newController->right);
                        
                        Win32ProcessXInputButton(pad->wButtons,
                                                 XINPUT_GAMEPAD_LEFT_SHOULDER,
                                                 &oldController->leftShoulder,
                                                 &newController->leftShoulder);
                        
                        Win32ProcessXInputButton(pad->wButtons,
                                                 XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                                 &oldController->rightShoulder,
                                                 &newController->rightShoulder);
                    }
                    else
                    {
                        // NOTE(yuval & eran): The contorller is not connected
                        // TODO(yuval & eran): Better log
                        // LogInfo("Controller: %u is not connected!", controller);
                    }
                }
                
                b32 soundIsValid = false;
                
                DWORD playCursor = 0;
                DWORD writeCursor = 0;
                DWORD byteToLock = 0;
                DWORD bytesToWrite = 0;
                
                if (SUCCEEDED(globalSeconderyBuffer->
                              GetCurrentPosition(&playCursor, &writeCursor)))
                {
                    soundIsValid = true;
                    
                    byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample)
                        % soundOutput.seconderyBufferSize;
                    
                    if (byteToLock > playCursor)
                    {
                        bytesToWrite = soundOutput.seconderyBufferSize - byteToLock;
                        bytesToWrite += playCursor;
                    }
                    else
                    {
                        bytesToWrite = playCursor - byteToLock;
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
                
                Win32WindowDimension dim = Win32GetWindowDimension(window);
                Win32DisplayBackbufferInWindow(deviceContext, &globalBackbuffer, dim.width, dim.height);
                
#if 0
                u64 endCycleCount = __rdtsc();
                
                LARGE_INTEGER endCounter;
                QueryPerformanceCounter(&endCounter);
                
                u64 cyclesElapsed = endCycleCount - lastCycleCount;
                s64 counterElapsed = endCounter.QuadPart - lastCounter.QuadPart;
                
                r32 msPerFrame = (1000.0f * (r32)counterElapsed) / (r32)perfCountFrequency;
                r32 fps = (r32)perfCountFrequency / (r32)counterElapsed;
                r32 mcpf = (r32)cyclesElapsed / 1000000.0f;
                
                LogDebug("%.2fms/f,  %.2f/s,  %.2fmc/f", msPerFrame, fps, mcpf);
                
                lastCycleCount = endCycleCount;
                lastCounter = endCounter;
#endif
                
                // TODO(yuval & eran): Metaprogramming SWAP
                GameInput* temp = newInput;
                newInput = oldInput;
                oldInput = temp;
            }
        }
    }
    else
    {
        LogError("Window Class Registration Failed!");
    }
    
    return 0;
}

