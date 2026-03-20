#include "Window.h"
#include <iostream>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#endif

namespace cloth {

Window::Window(const WindowProps& props) : m_Props(props) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    // Hide window initially to avoid black flash during initialization
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    m_Window = glfwCreateWindow(
        m_Props.width,
        m_Props.height,
        m_Props.title.c_str(),
        nullptr,
        nullptr
    );

    if (!m_Window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return;
    }

    glfwMakeContextCurrent(m_Window);
    // Disable vsync for smoother rendering
    glfwSwapInterval(0);
    
#ifdef _WIN32
    // Extra hide on Windows to prevent any flash
    HWND hwnd = glfwGetWin32Window(m_Window);
    ShowWindow(hwnd, SW_HIDE);
#endif
    
    // Window will be shown after first frame is rendered in Application::Run()
}

Window::~Window() {
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

void Window::OnUpdate() {
    glfwPollEvents();
}

bool Window::ShouldClose() const {
    return glfwWindowShouldClose(m_Window);
}

void Window::Terminate() {
    glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
}

void Window::PollEvents() {
    glfwPollEvents();
}

void Window::WaitEvents() {
    glfwWaitEvents();
}

} // namespace cloth
