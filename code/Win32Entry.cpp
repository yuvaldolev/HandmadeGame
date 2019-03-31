#include <windows.h>

#define internal static
#define global_variable static
#define local_persist static

// TODO(yuval & eran): Remove global variable!!!
global_variable bool running;

global_variable BITMAPINFO bitmapInfo;
global_variable void* bitmapMemory;
global_variable HBITMAP bitmapHandle;
global_variable HDC bitmapDeviceContext;

internal void Win32ResizeDIBSection(int width, int height)
{
    if (bitmapHandle)
    {
        DeleteObject(bitmapHandle);
    }

    if (!bitmapDeviceContext)
    {
        bitmapDeviceContext = CreateCompatibleDC(0);
    }

    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    bitmapHandle = CreateDIBSection(
        bitmapDeviceContext,
        &bitmapInfo,
        DIB_RGB_COLORS,
        &bitmapMemory,
        0, 0);
}

internal void Win32UpdateWindow(HDC deviceContext,
                                int x, int y,
                                int width, int height)
{
    StretchDIBits(deviceContext,
                  x, y, width, height,
                  x, y, width, height,
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

            Win32UpdateWindow(deviceContext, x, y, width, height);
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
        HWND windowHandle = CreateWindowEx(
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

        if (windowHandle)
        {
            running = true;

            while (running)
            {
                MSG message;
                BOOL messageResult = GetMessage(&message, 0, 0, 0);

                if (messageResult > 0)
                {
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }
                else
                {
                    break;
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

