#include <windows.h>
#include <stdint.h>

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

// TODO(yuval & eran): Remove global variable!!!
global_variable bool running;

global_variable BITMAPINFO bitmapInfo;
global_variable void* bitmapMemory;
global_variable int bitmapWidth;
global_variable int bitmapHeight;
global_variable int bytesPerPixel = 4;

internal void RenderGradient(int xOffset, int yOffset)
{
    int pitch = bitmapWidth * bytesPerPixel;
    uint8* row = (uint8*)bitmapMemory;

    for (int y = 0; y < bitmapHeight; ++y)
    {
        uint8* pixel = row;

        for (int x = 0; x < bitmapWidth; ++x)
        {
            *pixel = (uint8)(x + xOffset);
            ++pixel;

            *pixel = (uint8)(y + yOffset);
            ++pixel;

            *pixel = 0;
            ++pixel;

            *pixel = 0;
            ++pixel;
        }

        row += pitch;
    }
}
internal void Win32ResizeDIBSection(int width, int height)
{
    if (bitmapMemory)
    {
        VirtualFree(bitmapMemory, 0, MEM_RELEASE);
    }

    bitmapWidth = width;
    bitmapHeight = height;

    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = bitmapWidth;
    bitmapInfo.bmiHeader.biHeight = -bitmapHeight;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;


    int bitmapMemorySize = bitmapWidth * bitmapHeight * bytesPerPixel;

    bitmapMemory = VirtualAlloc(0, bitmapMemorySize,
                                MEM_COMMIT, PAGE_READWRITE);
}

internal void Win32UpdateWindow(HDC deviceContext,
                                int x, int y)
{
    StretchDIBits(deviceContext,
                  0, 0, bitmapWidth, bitmapHeight,
                  0, 0, bitmapWidth, bitmapHeight,
                  bitmapMemory,
                  &bitmapInfo,
                  DIB_RGB_COLORS,
                  SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(HWND window,
                                         UINT message,
                                         WPARAM wParam,
                                         LPARAM lParam)
{
    LRESULT result = 0;

    switch (message)
    {
        case WM_SIZE:
        {
            RECT clientRect;
            GetClientRect(window, &clientRect);

            // NOTE(yuval & eran): Duplicated code, refactor into function
            int width = clientRect.right - clientRect.left;
            int height = clientRect.bottom - clientRect.top;

            Win32ResizeDIBSection(width, height);
        } break;

        case WM_DESTROY:
        {
            running = false;
        } break;

        case WM_CLOSE:
        {
            running = false;
        } break;

        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("Window Refocused\n");
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint(window, &paint);

            int x = paint.rcPaint.left;
            int y = paint.rcPaint.top;
            int width = paint.rcPaint.right - paint.rcPaint.left;
            int height = paint.rcPaint.bottom - paint.rcPaint.top;

            Win32UpdateWindow(deviceContext, x, y);
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
            running = true;

            int xOffset = 0;
            int yOffset = 0;

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

                RenderGradient(xOffset++, yOffset++);

                HDC deviceContext = GetDC(window);
                Win32UpdateWindow(deviceContext, 0, 0);
                ReleaseDC(window, deviceContext);
            }
        }
    }
    else
    {
        // TODO(yuval & eran): Logging!
    }

    return 0;
}

