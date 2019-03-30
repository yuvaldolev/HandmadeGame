#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nShowCmd)
{
    MessageBox(0, "Hello", "Darkness", MB_OK | MB_ICONINFORMATION);
    return 0;
}

