#include "window.h"
#include <stdexcept>
#include <string.h>
#include "engine.h"

#if(_WIN32)
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

void Winodw::handleEvents()
{
    MSG msg {};
    GetMessage(&msg, NULL, 0, 0);
    TranslateMessage(&msg);
    DispatchMessage(&msg);
}
#elif(__linux__)

namespace global
{
    static wl_compositor* compositor = nullptr;

    static void registry_handler(void*, wl_registry* registry, uint32_t id, const char* interface, uint32_t)
    {
        if(strcmp(interface, "wl_compositor") == 0)
        {
            compositor = static_cast<wl_compositor*>(wl_registry_bind(registry, id, &wl_compositor_interface, 1));
        }
    }

    static void registry_remover(void*, wl_registry*, uint32_t)
    {
        // ???
    }

    static const wl_registry_listener registry_listeners = {registry_handler, registry_remover};
}


Window::Window(Engine *enginePointer)
{
    mEnginePointer = enginePointer;
    mDisplay = wl_display_connect(NULL);
    if(mDisplay == NULL)
    {
        throw std::runtime_error("failed to connect with wayland server");
    }

    wl_registry* registry = wl_display_get_registry(mDisplay);
    wl_registry_add_listener(registry, &global::registry_listeners, NULL);
    wl_display_dispatch(mDisplay);
    wl_display_roundtrip(mDisplay);

    if(global::compositor == NULL)
    {
        throw std::runtime_error("failed to find a compositor");
    }

    mSurface = wl_compositor_create_surface(global::compositor);
    if(mSurface == NULL)
    {
        throw std::runtime_error("failed to create a surface");
    }
}

Window::~Window()
{
    wl_display_disconnect(mDisplay);
}

wl_surface* Window::getSurface()
{
    return mSurface;
}

wl_display* Window::getDisplay()
{
    return mDisplay;
}

void Window::handleEvents()
{

}

#endif
