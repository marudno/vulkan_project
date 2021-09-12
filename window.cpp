#include "window.h"
#include <stdexcept>
#include <string.h>
#include "engine.h"

#if(_WIN32)
namespace global
{
    static Window* windowPointer = nullptr;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
// funkcja wywoływana za każdym razem gdy pojawiają się dane wejściowe (uMsg). wParam posiada informację jaki klawisz został wciśnięty
{
    return global::windowPointer->myWindowProc(hwnd, uMsg, wParam, lParam);
}

Window::Window(Engine* enginePointer, uint16_t windowWidth, uint16_t windowHeight)
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
    uint32_t displayWidth = GetDeviceCaps(hdc, HORZRES);
    uint32_t displayHeight = GetDeviceCaps(hdc, VERTRES);
    ReleaseDC(NULL, hdc);

    /*------ create window --------*/
    mHwnd = CreateWindowEx(0, mClassName, "", WS_POPUP | WS_CLIPCHILDREN, (displayWidth - windowWidth)/2, (displayHeight - windowHeight)/2, windowWidth, windowHeight, NULL, NULL, mHinstance, NULL);

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

void Window::handleEvents()
{
    MSG msg {};
    GetMessage(&msg, NULL, 0, 0);
    TranslateMessage(&msg);
    DispatchMessage(&msg);
}
#elif(__linux__)

Window::Window(Engine* enginePointer, uint16_t windowWidth, uint16_t windowHeight)
{
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t borderWidth = 10;

    mEnginePointer = enginePointer;
    xcb_screen_t* screen;

    mConnection = xcb_connect(NULL, NULL);
    if(auto error = xcb_connection_has_error((mConnection)); error != 0)
    {
        xcb_disconnect(mConnection);
        throw(std::runtime_error("failed to xcb connection to the X server"));
    }

    screen = xcb_setup_roots_iterator(xcb_get_setup(mConnection)).data;
    mWindowId = xcb_generate_id(mConnection);

    xcb_create_window(mConnection, XCB_COPY_FROM_PARENT, mWindowId, screen->root, x, y, windowWidth, windowHeight, borderWidth, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, 0, NULL);

    xcb_map_window(mConnection, mWindowId);
    xcb_flush(mConnection);
}

Window::~Window()
{
    xcb_disconnect(mConnection);
}

void Window::handleEvents()
{

}
#endif
