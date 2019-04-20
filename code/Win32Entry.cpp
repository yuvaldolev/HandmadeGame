#include <windows.h>
#include <dsound.h>
#include <xinput.h>

#include <stdio.h> // TODO(yuval & eran): TEMPORARY
#include <math.h>

#include "Game.cpp"

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
    s32 samplesPerSecond = 48000;
    s32 toneHz = 256;
    s32 toneVolume = 16000;
    u32 runningSampleIndex = 0;
    s32 wavePeriod = samplesPerSecond / toneHz;
    s32 halfWavePeriod = wavePeriod / 2;
    s32 bytesPerSample = sizeof(s16) * 2;
    s32 seconderyBufferSize = samplesPerSecond * bytesPerSample;
};

// TODO(yuval & eran): Remove global variable!!!
global_variable bool globalRunning;
global_variable Win32Backbuffer globalBackbuffer;
global_variable IDirectSoundBuffer* globalSeconderyBuffer;

global_variable s32 globalXOffset = 0;
global_variable s32 globalYOffset = 0;

internal void
RenderGradient(Win32Backbuffer* buffer, s32 xOffset, s32 yOffset)
{
    u8* row = (u8*)buffer->memory;

    for (s32 y = 0; y < buffer->height; ++y)
    {
        u32* pixel = (u32*)row;

        for (s32 x = 0; x < buffer->width; ++x)
        {
            u8 Blue = (u8)(x + xOffset);
            u8 Green = (u8)(y + yOffset);

            *pixel++ = (Green << 8) | Blue;
        }

        row += buffer->pitch;
    }
}

internal void
Win32FillSoundBuffer(IDirectSoundBuffer* soundBuffer,
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

        // NOTE(yuval): Writing a square wave to region 1
        s32 region1SampleCount = region1Size / soundOutput->bytesPerSample;
        s16* sampleOut = (s16*)region1;
        for (DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex)
        {
            r32 t = 2 * Pi32 * ((r32)soundOutput->runningSampleIndex / (r32)soundOutput->wavePeriod);
            r32 sineValue = sinf(t);
            s16 sampleValue = (s16)(sineValue * soundOutput->toneVolume);
            *sampleOut++ = sampleValue;
            *sampleOut++ = sampleValue;

            ++soundOutput->runningSampleIndex;
        }

        // NOTE(yuval): Writing a square wave to region 2
        s32 region2SampleCount = region2Size / soundOutput->bytesPerSample;
        sampleOut = (s16*)region2;
        for (DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex)
        {
            r32 t = 2 * Pi32 * ((r32)soundOutput->runningSampleIndex / (r32)soundOutput->wavePeriod);
            r32 sineValue = sinf(t);
            s16 sampleValue = (s16)(sineValue * soundOutput->toneVolume);
            *sampleOut++ = sampleValue;
            *sampleOut++ = sampleValue;

            ++soundOutput->runningSampleIndex;
        }

        globalSeconderyBuffer->Unlock(region1, region1Size,
                                      region2, region2Size);
    }
    else
    {
        // TODO(yuval & eran): Diagnostics
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
                        OutputDebugStringA("Primary buffer format was set\n");
                    }
                    else
                    {
                        // TODO(yuval & eran): Diagnostics
                    }
                }
            }
            else
            {
                // TODO(yuval & eran): Diagnostics
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
                OutputDebugStringA("Second Buffer Created!\n");
            }
        }
        else
        {
            // TODO(yuval & eran): Diagnostics
        }
    }
    else
    {
        // TODO(yuval & eran): Diagnostics
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
        // TODO(yuval & eran): Diagnostics
    }
}

// TODO(yuval & eran): This is temporary
internal void
Win32OpenConsole()
{
    AllocConsole();
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
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
                                MEM_COMMIT, PAGE_READWRITE);

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
            u32 keyCode = wParam;
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
                        globalYOffset += 15;
                    } break;

                    case 'A':
                    {
                        globalXOffset += 15;
                    } break;

                    case 'S':
                    {
                        globalYOffset -= 15;
                    } break;

                    case 'D':
                    {
                        globalXOffset -= 15;
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
                        OutputDebugStringA("Space: ");

                        if (wasDown)
                        {
                            OutputDebugStringA("WasDown ");
                        }

                        if (isDown)
                        {
                            OutputDebugStringA("IsDown");
                        }

                        OutputDebugStringA("\n");
                    } break;
                }
            }
        } break;

        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("Window Refocused\n");
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
    // TODO(yuval & eran): This is temporary
    Win32OpenConsole();

    //LogInit(LogLevelDebug, "[%V] [%d] %f:%U:%L - %m%n");
    LogInit(LogLevelDebug, "%m");

    LogDebug("Name: %s, Age: %d", "Bob", 40);

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
            soundOutput.toneHz = 256;
            soundOutput.toneVolume = 16000;
            soundOutput.runningSampleIndex = 0;
            soundOutput.wavePeriod = soundOutput.samplesPerSecond /
                soundOutput.toneHz;
            soundOutput.halfWavePeriod = soundOutput.wavePeriod / 2;
            soundOutput.bytesPerSample = sizeof(s16) * 2;
            soundOutput.seconderyBufferSize = soundOutput.samplesPerSecond *
                soundOutput.bytesPerSample;

            Win32InitDSound(window, soundOutput.samplesPerSecond,
                            soundOutput.seconderyBufferSize);

            Win32FillSoundBuffer(globalSeconderyBuffer, &soundOutput,
                                 0, soundOutput.seconderyBufferSize);
            globalSeconderyBuffer->Play(0, 0, DSBPLAY_LOOPING);

            globalRunning = true;

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

                for (DWORD controller = 0; controller < XUSER_MAX_COUNT; ++controller)
                {
                    XINPUT_STATE controllerState;

                    if (XInputGetState(controller, &controllerState) == ERROR_SUCCESS)
                    {
                        XINPUT_GAMEPAD* pad = &controllerState.Gamepad;

                        bool up = pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        bool down = pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                        bool left = pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        bool right = pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

                        bool start = pad->wButtons & XINPUT_GAMEPAD_START;
                        bool back = pad->wButtons & XINPUT_GAMEPAD_BACK;

                        bool leftShoulder = pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
                        bool rightShoulder = pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;

                        bool aButton = pad->wButtons & XINPUT_GAMEPAD_A;
                        bool bButton = pad->wButtons & XINPUT_GAMEPAD_B;
                        bool xButton = pad->wButtons & XINPUT_GAMEPAD_X;
                        bool yButton = pad->wButtons & XINPUT_GAMEPAD_Y;

                        s16 stickX = pad->sThumbLX;
                        s16 stickY = pad->sThumbLY;

                        if (aButton)
                        {
                            globalYOffset -= 5;
                        }
                        else if(yButton)
                        {
                            globalYOffset += 5;
                        }
                        else if (xButton)
                        {
                            globalXOffset += 5;
                        }
                        else if (bButton)
                        {
                            globalXOffset -= 5;
                        }

                        globalXOffset -= stickX / 2000;
                        globalYOffset += stickY / 2000;
                    }
                    else
                    {
                        // NOTE(yuval & eran): The contorller is not connected
                        // TODO(yuval & eran): Diagnostics
                    }
                }

                // NOTE(yuval): Render test
                RenderGradient(&globalBackbuffer, globalXOffset, globalYOffset);

                Win32WindowDimension dim = Win32GetWindowDimension(window);
                Win32DisplayBackbufferInWindow(deviceContext, &globalBackbuffer, dim.width, dim.height);

                // NOTE(yuval): DirectSound output test
                DWORD playCursor;
                DWORD writeCursor;
                if (SUCCEEDED(globalSeconderyBuffer->
                              GetCurrentPosition(&playCursor, &writeCursor)))
                {
                    DWORD byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample)
                        % soundOutput.seconderyBufferSize;
                    DWORD bytesToWrite;

                    b32 isPlaying = true;

                    if (byteToLock == playCursor)
                    {
                        bytesToWrite = 0;
                    }
                    else if (byteToLock > playCursor)
                    {
                        bytesToWrite = soundOutput.seconderyBufferSize - byteToLock;
                        bytesToWrite += playCursor;
                    }
                    else
                    {
                        bytesToWrite = playCursor - byteToLock;
                    }

                    Win32FillSoundBuffer(globalSeconderyBuffer, &soundOutput,
                                         byteToLock, bytesToWrite);
                }
            }
        }
    }
    else
    {
        // TODO(yuval & eran): Logging!
    }

    return 0;
}

