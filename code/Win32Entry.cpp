#include <windows.h>

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
            OutputDebugStringA("Window Resized\n");
        } break;

        case WM_DESTROY:
        {
            OutputDebugStringA("Window Destroyed\n");
        } break;

        case WM_CLOSE:
        {
            OutputDebugStringA("User Wants To Close Window\n");
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
            MSG message;

            while (true)
            {
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

