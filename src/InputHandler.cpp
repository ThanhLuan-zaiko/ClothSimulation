#include "InputHandler.h"
#include <glm/gtc/matrix_transform.hpp>

void HandleInput(AppState& state, float deltaTime) {

    // Camera controls - increased base speed for faster movement
    float baseCameraSpeed = 30.0f * deltaTime;
    float cameraSpeed = Input::IsKeyPressed(GLFW_KEY_LEFT_SHIFT) || Input::IsKeyPressed(GLFW_KEY_RIGHT_SHIFT)
                       ? baseCameraSpeed * 4.0f  // Sprint mode - very fast
                       : baseCameraSpeed;

    if (Input::IsKeyPressed(GLFW_KEY_W) || Input::IsKeyPressed(GLFW_KEY_UP)) {
        state.camera.MoveForward(cameraSpeed);
    }
    if (Input::IsKeyPressed(GLFW_KEY_S) || Input::IsKeyPressed(GLFW_KEY_DOWN)) {
        state.camera.MoveForward(-cameraSpeed);
    }
    if (Input::IsKeyPressed(GLFW_KEY_A) || Input::IsKeyPressed(GLFW_KEY_LEFT)) {
        state.camera.MoveRight(-cameraSpeed);
    }
    if (Input::IsKeyPressed(GLFW_KEY_D) || Input::IsKeyPressed(GLFW_KEY_RIGHT)) {
        state.camera.MoveRight(cameraSpeed);
    }

    // Camera zoom with scroll wheel - faster zoom
    float scrollSpeed = 40.0f * deltaTime;
    if (Input::IsKeyPressed(GLFW_KEY_EQUAL) || Input::IsKeyPressed(GLFW_KEY_PAGE_UP)) {
        state.cameraFOV -= scrollSpeed;
        if (state.cameraFOV < 10.0f) state.cameraFOV = 10.0f;
    }
    if (Input::IsKeyPressed(GLFW_KEY_MINUS) || Input::IsKeyPressed(GLFW_KEY_PAGE_DOWN)) {
        state.cameraFOV += scrollSpeed;
        if (state.cameraFOV > 90.0f) state.cameraFOV = 90.0f;
    }

    // Mouse look - reset position on first click to prevent jump
    if (Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) && !state.mousePressed) {
        state.mousePressed = true;
        state.lastMouseX = Input::GetMouseX();
        state.lastMouseY = Input::GetMouseY();
    }
    if (!Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        state.mousePressed = false;
    }
    if (state.mousePressed) {
        double currentX = Input::GetMouseX();
        double currentY = Input::GetMouseY();

        float deltaX = static_cast<float>(currentX - state.lastMouseX);
        float deltaY = static_cast<float>(currentY - state.lastMouseY);

        // Mouse sensitivity
        state.cameraYaw += deltaX * 0.2f;
        state.cameraPitch -= deltaY * 0.2f;

        if (state.cameraPitch > 89.0f) state.cameraPitch = 89.0f;
        if (state.cameraPitch < -89.0f) state.cameraPitch = -89.0f;

        state.camera.SetRotation(glm::vec3(state.cameraYaw, state.cameraPitch, 0.0f));

        state.lastMouseX = currentX;
        state.lastMouseY = currentY;
    }

    // Reset all cloths
    if (Input::IsKeyPressed(GLFW_KEY_SPACE)) {
        for (auto* c : state.cloths) {
            c->Reset();
        }
    }

    // Update key repeat timer
    state.keyRepeatTimer -= deltaTime;

    // Terrain texture selection - FAST response (no blocking while loops)
    bool tKeyPressed = Input::IsKeyPressed(GLFW_KEY_T);
    bool shiftKeyPressed = Input::IsKeyPressed(GLFW_KEY_LEFT_SHIFT) || Input::IsKeyPressed(GLFW_KEY_RIGHT_SHIFT);

    if (tKeyPressed && !state.lastTKeyPressed) {
        // First press - immediate action
        if (shiftKeyPressed) {
            state.world.PreviousTexture();
        } else {
            state.world.NextTexture();
        }
        state.reflectionNeedsUpdate = true; // Optimization: Trigger reflection update
        state.keyRepeatTimer = state.keyRepeatDelay;
    } else if (tKeyPressed && state.keyRepeatTimer <= 0) {
        // Key repeat
        if (shiftKeyPressed) {
            state.world.PreviousTexture();
        } else {
            state.world.NextTexture();
        }
        state.reflectionNeedsUpdate = true; // Optimization: Trigger reflection update
        state.keyRepeatTimer = state.keyRepeatDelay;
    }
    state.lastTKeyPressed = tKeyPressed;
    state.lastShiftKeyPressed = shiftKeyPressed;

    // Direct texture selection with number keys - FAST response
    int textureCount = state.world.GetTextureCount();
    for (int i = 0; i < textureCount && i < 9; ++i) {
        bool numKeyPressed = Input::IsKeyPressed(GLFW_KEY_1 + i);
        if (numKeyPressed && !state.lastNumberKeysPressed[i]) {
            state.world.SetCurrentTexture(i);
            state.reflectionNeedsUpdate = true; // Optimization: Trigger reflection update
            state.keyRepeatTimer = state.keyRepeatDelay;
        }
        state.lastNumberKeysPressed[i] = numKeyPressed;
    }

    // Toggle terrain wireframe - FAST response
    bool fKeyPressed = Input::IsKeyPressed(GLFW_KEY_F);
    if (fKeyPressed && !state.lastFKeyPressed) {
        state.world.GetTerrain().SetWireframe(!state.world.GetTerrain().IsWireframe());
    }
    state.lastFKeyPressed = fKeyPressed;

    // Update camera FOV
    state.camera.SetFOV(state.cameraFOV);
}
