#include "Input.h"
#include "Window.h"

// Forward declaration - sẽ được set bởi Application
extern GLFWwindow* g_WindowHandle;

namespace cloth {

bool Input::IsKeyPressed(int keyCode) {
    if (!g_WindowHandle) return false;
    return glfwGetKey(g_WindowHandle, keyCode) == GLFW_PRESS;
}

bool Input::IsMouseButtonPressed(int button) {
    if (!g_WindowHandle) return false;
    return glfwGetMouseButton(g_WindowHandle, button) == GLFW_PRESS;
}

glm::vec2 Input::GetMousePosition() {
    if (!g_WindowHandle) return glm::vec2(0.0f);
    double xpos, ypos;
    glfwGetCursorPos(g_WindowHandle, &xpos, &ypos);
    return glm::vec2(static_cast<float>(xpos), static_cast<float>(ypos));
}

double Input::GetMouseX() {
    return GetMousePosition().x;
}

double Input::GetMouseY() {
    return GetMousePosition().y;
}

void Input::SetMousePosition(double x, double y) {
    if (g_WindowHandle) {
        glfwSetCursorPos(g_WindowHandle, x, y);
    }
}

} // namespace cloth
