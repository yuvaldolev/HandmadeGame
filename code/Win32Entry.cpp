#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <stdio.h>

#define internal static
#define global_variable static
#define local_persist static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(XInputGetStateType);

X_INPUT_GET_STATE(XInputGetStateStub)
{
    return !ERROR_SUCCESS;
}

global_variable XInputGetStateType* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(XInputSetStateType);

X_INPUT_SET_STATE(XInputSetStateStub)
{
    return !ERROR_SUCCESS;
}

global_variable XInputSetStateType* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

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

// TODO(yuval & eran): Remove global variable!!!
global_variable bool running;
global_variable Win32Backbuffer globalBackbuffer;

global_variable int GlobalXOffset = 0;
global_variable int GlobalYOffset = 0;

internal void RenderGradient(Win32Backbuffer* buffer, int xOffset, int yOffset)
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

internal void Win32LoadXInput()
{
    // TODO(yuval): Link to xinput_1_3.dll
    HMODULE XInputLibrary = LoadLibrary(XINPUT_DLL_A);

    if (XInputLibrary)
    {
        XInputGetState = (XInputGetStateType*)GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetState = (XInputSetStateType*)GetProcAddress(XInputLibrary, "XInputSetState");
    }
}

internal Win32WindowDimension Win32GetWindowDimension(HWND window)
{
    Win32WindowDimension dim = { };

    RECT clientRect;
    GetClientRect(window, &clientRect);

    // NOTE(yuval & eran): Duplicated code, refactor into function
    dim.width = clientRect.right - clientRect.left;
    dim.height = clientRect.bottom - clientRect.top;

    return dim;
}

internal void Win32ResizeDIBSection(Win32Backbuffer* buffer, int width, int height)
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

internal void Win32DisplayBackbufferInWindow(HDC deviceContext, Win32Backbuffer* buffer,
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

internal LRESULT CALLBACK Win32MainWindowCallback(HWND window,
                                                  UINT message,
                                                  WPARAM wParam,
                                                  LPARAM lParam)
{
    LRESULT result = 0;

    switch (message)
    {
        case WM_SIZE:
        {
        } break;

        case WM_DESTROY:
        {
            running = false;
        } break;

        case WM_CLOSE:
        {
            running = false;
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            uint32 keyCode = wParam;
            bool wasDown = (lParam & (1 << 30)) != 0;
            bool isDown = (lParam & (1 << 31)) == 0;

            switch (keyCode)
            {
                case 'W':
                {
                    GlobalYOffset += 15;
                } break;

                case 'A':
                {
                    GlobalXOffset += 15;
                } break;

                case 'S':
                {
                    GlobalYOffset -= 15;
                } break;

                case 'D':
                {
                    GlobalXOffset -= 15;
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

int WINAPI WinMain(HINSTANCE instance,
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

    if (RegisterClass(&windowClass))
    {
        HWND window = CreateWindowEx(
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

            running = true;

            while (running)
            {
                MSG message;

                while (PeekMessage(&message, window, 0, 0, PM_REMOVE))
                {
                    if (message.message == WM_QUIT)
                    {
                        running = false;
                    }

                    TranslateMessage(&message);
                    DispatchMessage(&message);
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
                            GlobalYOffset -= 5;
                        }
                        else if(yButton)
                        {
                            GlobalYOffset += 5;
                        }
                        else if (xButton)
                        {
                            GlobalXOffset += 5;
                        }
                        else if (bButton)
                        {
                            GlobalXOffset -= 5;
                        }

                        GlobalXOffset -= stickX / 2000;
                        GlobalYOffset += stickY / 2000;
                    }
                    else
                    {
                        // NOTE(yuval & eran): The contorller is not connected
//                        OutputDebugStringA("Controller Off\n");
                    }
                }

//                OutputDebugStringA("\n");

                RenderGradient(&globalBackbuffer, GlobalXOffset, GlobalYOffset);

                Win32WindowDimension dim = Win32GetWindowDimension(window);
                Win32DisplayBackbufferInWindow(deviceContext, &globalBackbuffer, dim.width, dim.height);
            }
        }
    }
    else
    {
        // TODO(yuval & eran): Logging!
    }

    return 0;
}

