#pragma once

// Prevent GLFW from including OpenGL headers - GLAD handles that
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <string>

namespace cloth {

struct WindowProps {
    std::string title;
    unsigned int width;
    unsigned int height;

    WindowProps(const std::string& title = "Cloth Simulation",
                unsigned int width = 1280,
                unsigned int height = 720)
        : title(title), width(width), height(height) {}
};

class Window {
public:
    Window(const WindowProps& props);
    ~Window();

    void OnUpdate();
    bool ShouldClose() const;
    void Terminate();

    GLFWwindow* GetNativeWindow() const { return m_Window; }
    unsigned int GetWidth() const { return m_Props.width; }
    unsigned int GetHeight() const { return m_Props.height; }

    static void PollEvents();
    static void WaitEvents();

private:
    GLFWwindow* m_Window = nullptr;
    WindowProps m_Props;
};

} // namespace cloth
