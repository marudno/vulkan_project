#ifndef WINDOW_H
#define WINDOW_H
#include <windows.h>

class Engine; //bo w engine include window.h, nie można zaincludować tutaj engine.h bo zrobi się pętla, więc trzeba stworzyć defenicję

class Window
{
public:
    Window(Engine* mEnginePointer);
    ~Window();
    HINSTANCE getHinstance() const; //chcemy by funkcja była const - aby nie zmieniła hinstance
    HWND getHwnd() const;
    LRESULT CALLBACK myWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
private:
    const char* mClassName = "Vulkan Project";
    HINSTANCE mHinstance = GetModuleHandle(NULL);
    HWND mHwnd = NULL;
    Engine* mEnginePointer = nullptr;
};

#endif // WINDOW_H
