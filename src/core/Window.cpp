#include "Window.h"
#include <iostream>

namespace cloth {

Window::Window(const WindowProps& props) : m_Props(props) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, 1); // GL_TRUE = 1

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
    glfwSwapInterval(1); // Enable vsync
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
