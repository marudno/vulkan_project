#include "windows.h"
#include "window.h"
#include <stdexcept>
#include "engine.h"

namespace global
{
    static Window* windowPointer = nullptr; // static - ma dokładnie jedną inicjalizację i istnieje przez cały czas trwania programu
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
// funkcja wywoływana za każdym razem gdy pojawiają się dane wejściowe (uMsg). wParam posiada informację jaki klawisz został wciśnięty
{
    return global::windowPointer->myWindowProc(hwnd, uMsg, wParam, lParam);
}

Window::Window(Engine* enginePointer)
{
    mEnginePointer = enginePointer;
    global::windowPointer = this;

    /*--- register window class ---*/
    WNDCLASS windowClass {};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = mHinstance;
    windowClass.lpszClassName = mClassName;
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RegisterClass(&windowClass);

    /*-------- resolution ---------*/
    HDC hdc = GetDC(NULL); // get screen DC
    uint32_t displayWidth = GetDeviceCaps(hdc, HORZRES); // wide
    uint32_t displayHeight = GetDeviceCaps(hdc, VERTRES); // high
    ReleaseDC(NULL, hdc);

    /*------ create window --------*/
    uint32_t windowWidth = 800;
    uint32_t windowHeight = 600;
    mHwnd = CreateWindowEx(0, mClassName, "", WS_POPUP | WS_CLIPCHILDREN, (displayWidth - windowWidth)/2, (displayHeight - windowHeight)/2, 800, 600, NULL, NULL, mHinstance, NULL);

    if(mHwnd == NULL) //jeśli createwindow zwróci null - okno nie zostało stworzone
    {
        throw std::runtime_error("hwnd = 0");
    }

    ShowWindow(mHwnd, SW_SHOW);
}

Window::~Window()
{
    UnregisterClass(mClassName, mHinstance);
}

HINSTANCE Window::getHinstance() const
{
    return mHinstance;
}

HWND Window::getHwnd() const
{
    return mHwnd;
}

LRESULT Window::myWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }
    case WM_KEYDOWN:
        if(wParam == VK_ESCAPE) //przyrównujemy do wParam, bo ma informację o tym jaki klawisz jest wciśnięty
        {
            DestroyWindow(hwnd);
            mEnginePointer->stop();
        }
        return 0;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}
