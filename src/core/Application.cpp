#include "Application.h"
#include <glad/glad.h>
#include <iostream>

// Global window handle cho Input
GLFWwindow* g_WindowHandle = nullptr;

namespace cloth {

Application::Application()
    : m_Running(true), m_DeltaTime(0.0f), m_LastFrameTime(0.0f) {
    
    m_Window = new Window(WindowProps("Cloth Simulation", 1280, 720));
    g_WindowHandle = m_Window->GetNativeWindow();

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return;
    }

    // OpenGL configuration
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
}

Application::~Application() {
    delete m_Window;
    g_WindowHandle = nullptr;
}

void Application::Run() {
    while (m_Running && !m_Window->ShouldClose()) {
        // Calculate delta time
        float currentFrame = static_cast<float>(glfwGetTime());
        m_DeltaTime = currentFrame - m_LastFrameTime;
        m_LastFrameTime = currentFrame;

        // Clear buffers
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Update
        if (m_UpdateCallback) {
            m_UpdateCallback(m_DeltaTime);
        }

        // Render
        if (m_RenderCallback) {
            m_RenderCallback();
        }

        // Swap buffers and poll events
        m_Window->OnUpdate();
        glfwSwapBuffers(m_Window->GetNativeWindow());
    }
}

void Application::Quit() {
    m_Running = false;
}

} // namespace cloth
