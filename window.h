#ifndef WINDOW_H
#define WINDOW_H

class Engine; //bo w engine include window.h

#if(_WIN32)
#include <windows.h>
#define VK_USE_PLATFORM_WIN32_KHR 1
#define VK_PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME

class Window
{
public:
    Window(Engine* enginePointer);
    ~Window();
    HINSTANCE getHinstance() const; //chcemy by funkcja była const - aby nie zmieniła hinstance
    HWND getHwnd() const;
    LRESULT CALLBACK myWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void handleEvents();
private:
    const char* mClassName = "Vulkan Project";
    HINSTANCE mHinstance = GetModuleHandle(NULL);
    HWND mHwnd = NULL;
    Engine* mEnginePointer = nullptr;
};
#elif(__linux__)
#include <wayland-client.h>
#define VK_USE_PLATFORM_WAYLAND_KHR 1
#define VK_PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
class Window
{
public:
    Window(Engine* enginePointer);
    ~Window();
    wl_surface* getSurface();
    wl_display* getDisplay();
    void handleEvents();
    uint32_t WIDTH = 800;
    static const uint32_t HEIGHT = 600;

private:
    Engine* mEnginePointer = nullptr;
    wl_display* mDisplay = nullptr;
    wl_surface* mSurface = nullptr;
    struct wls_hm_pool* mPool;
    void createWindow();
    wl_buffer* createBuffer();
};
#endif // _WIN32
#endif // WINDOW_H
