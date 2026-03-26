#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#endif

#include "Application.h"
#include "AppState.h"
#include <glad/glad.h>
#include <iostream>
#include "utils/GPUDetection.h"

// Global window handle cho Input
GLFWwindow* g_WindowHandle = nullptr;

// Global AppState pointer (defined in main.cpp)
namespace cloth {
    extern AppState* g_State;
}

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
    // Render first frame BEFORE showing window (avoid any black flash)
    // Clear with BRIGHT sky color
    glClearColor(0.6f, 0.75f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Call render callback to render full scene
    if (m_RenderCallback) {
        m_RenderCallback();
    }

    // Swap to display the rendered scene
    glfwSwapBuffers(m_Window->GetNativeWindow());

    // NOW show the window (on Windows, use API for smoother show)
#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(m_Window->GetNativeWindow());
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    // Force a small delay to ensure window is shown
    Sleep(10);
#else
    glfwShowWindow(m_Window->GetNativeWindow());
#endif

    while (m_Running && !m_Window->ShouldClose()) {
        // Calculate delta time
        float currentFrame = static_cast<float>(glfwGetTime());
        m_DeltaTime = currentFrame - m_LastFrameTime;
        m_LastFrameTime = currentFrame;

        // Clear buffers - use BRIGHT sky color to avoid black flash
        glClearColor(0.6f, 0.75f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Enable depth test with proper depth function
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Update
        if (m_UpdateCallback) {
            m_UpdateCallback(m_DeltaTime);
        }

        // Update adaptive quality system
        if (g_State && g_State->adaptiveQuality.IsEnabled()) {
            g_State->adaptiveQuality.Update(m_DeltaTime, static_cast<float>(glfwGetTime()));
        }

        // Render
        if (m_RenderCallback) {
            m_RenderCallback();
        }

        // Swap buffers and poll events
        glfwSwapBuffers(m_Window->GetNativeWindow());
        glfwPollEvents();
    }
}

void Application::Quit() {
    m_Running = false;
}

} // namespace cloth
