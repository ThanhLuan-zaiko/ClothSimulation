#pragma once

// Prevent GLFW from including OpenGL headers - GLAD handles that
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

namespace cloth {

class Input {
public:
    static bool IsKeyPressed(int keyCode);
    static bool IsMouseButtonPressed(int button);
    static glm::vec2 GetMousePosition();
    static double GetMouseX();
    static double GetMouseY();
    static void SetMousePosition(double x, double y);
};

} // namespace cloth
