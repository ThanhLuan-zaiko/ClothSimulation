#pragma once

#include "Window.h"
#include <functional>

namespace cloth {

class Application {
public:
    Application();
    ~Application();

    void Run();
    void Quit();

    Window& GetWindow() { return *m_Window; }

    // Set callbacks
    void SetUpdateCallback(std::function<void(float)> callback) { m_UpdateCallback = callback; }
    void SetRenderCallback(std::function<void()> callback) { m_RenderCallback = callback; }

private:
    Window* m_Window;
    bool m_Running;
    float m_DeltaTime;
    float m_LastFrameTime;

    std::function<void(float)> m_UpdateCallback;
    std::function<void()> m_RenderCallback;
};

} // namespace cloth
