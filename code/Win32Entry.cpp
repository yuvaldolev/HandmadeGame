#include <windows.h>
#include <dsound.h>
#include <xinput.h>

#include <stdint.h>
#include <math.h>

#define internal static
#define global_variable static
#define local_persist static

#define Pi32 3.14159265359

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

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
    int width;
    int height;
    int pitch;
    int bytesPerPixel;
};

struct Win32WindowDimension
{
    int width;
    int height;
};

struct Win32SoundOutput
{
    int samplesPerSecond = 48000;
    int toneHz = 256;
    int toneVolume = 16000;
    uint32 runningSampleIndex = 0;
    int wavePeriod = samplesPerSecond / toneHz;
    int halfWavePeriod = wavePeriod / 2;
    int bytesPerSample = sizeof(int16) * 2;
    int seconderyBufferSize = samplesPerSecond * bytesPerSample;
};

// TODO(yuval & eran): Remove global variable!!!
global_variable bool globalRunning;
global_variable Win32Backbuffer globalBackbuffer;
global_variable IDirectSoundBuffer* globalSeconderyBuffer;

global_variable int globalXOffset = 0;
global_variable int globalYOffset = 0;

internal void
RenderGradient(Win32Backbuffer* buffer, int xOffset, int yOffset)
{
    uint8* row = (uint8*)buffer->memory;

    for (int y = 0; y < buffer->height; ++y)
    {
        uint32* pixel = (uint32*)row;

        for (int x = 0; x < buffer->width; ++x)
        {
            uint8 Blue = (uint8)(x + xOffset);
            uint8 Green = (uint8)(y + yOffset);

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
        int region1SampleCount = region1Size / soundOutput->bytesPerSample;
        int16* sampleOut = (int16*)region1;
        for (DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex)
        {
            real32 t = 2 * Pi32 * ((real32)soundOutput->runningSampleIndex / (real32)soundOutput->wavePeriod);
            real32 sineValue = sinf(t);
            int16 sampleValue = (int16)(sineValue * soundOutput->toneVolume);
            *sampleOut++ = sampleValue;
            *sampleOut++ = sampleValue;

            ++soundOutput->runningSampleIndex;
        }

        // NOTE(yuval): Writing a square wave to region 2
        int region2SampleCount = region2Size / soundOutput->bytesPerSample;
        sampleOut = (int16*)region2;
        for (DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex)
        {
            real32 t = 2 * Pi32 * ((real32)soundOutput->runningSampleIndex / (real32)soundOutput->wavePeriod);
            real32 sineValue = sinf(t);
            int16 sampleValue = (int16)(sineValue * soundOutput->toneVolume);
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
Win32InitDSound(HWND window, int32 samplesPerSecond, int32 bufferSize)
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
Win32ResizeDIBSection(Win32Backbuffer* buffer, int width, int height)
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

    int bitmapMemorySize = buffer->width * buffer->height * buffer->bytesPerPixel;

    buffer->memory = VirtualAlloc(0, bitmapMemorySize,
                                MEM_COMMIT, PAGE_READWRITE);

    buffer->pitch = buffer->width * buffer->bytesPerPixel;
}

internal void
Win32DisplayBackbufferInWindow(HDC deviceContext, Win32Backbuffer* buffer,
                               int windowWidth, int windowHeight)
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
            uint32 keyCode = wParam;
            bool wasDown = (lParam & (1 << 30)) != 0;
            bool isDown = (lParam & (1 << 31)) == 0;

            bool32 altIsDown = lParam & (1 << 29);
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

int WINAPI
WinMain(HINSTANCE instance,
        HINSTANCE prevInstance,
        LPSTR commandLine,
        int showCode)
{
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
            soundOutput.bytesPerSample = sizeof(int16) * 2;
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

                        int16 stickX = pad->sThumbLX;
                        int16 stickY = pad->sThumbLY;

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

                    bool32 isPlaying = true;

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

