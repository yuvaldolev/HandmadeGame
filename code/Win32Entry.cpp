#include <windows.h>

#define internal static
#define global_variable static
#define local_persist static

// TODO(yuval & eran): Remove global variable!!!
global_variable bool running;

internal void Win32ResizeDIBSection()
{
    // NOTE(yuval & eran): Duplicated code, refactor into function
    int width = paint.rcPaint.right - paint.rcPaint.left;
    int height = paint.rcPaint.bottom - paint.rcPaint.top;


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

            Win32ResizeDIBSection();
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

            PatBlt(deviceContext, x, y, width, height, WHITENESS);

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

