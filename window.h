#ifndef WINDOW_H
#define WINDOW_H

class Engine;

#if(_WIN32)
#include <windows.h>
#include <stdint.h>
#define VK_USE_PLATFORM_WIN32_KHR 1
#define VK_PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME

class Window
{
public:
    Window(Engine* enginePointer, uint16_t windowWidth, uint16_t windowHeight);
    ~Window();
    HINSTANCE getHinstance() const;
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
#include <xcb/xcb.h>
#define VK_USE_PLATFORM_XCB_KHR 1
#define VK_PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_XCB_SURFACE_EXTENSION_NAME
class Window
{
public:
    Window(Engine* enginePointer, uint16_t width, uint16_t height);
    ~Window();

    xcb_connection_t* mConnection;
    xcb_window_t mWindowId;

    //TODO:
    void handleEvents();

private:
    Engine* mEnginePointer = nullptr;
};
#endif // _WIN32
#endif // WINDOW_H
