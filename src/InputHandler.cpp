#include "InputHandler.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "renderer/Renderer.h" // To get LoadTerrainTexture if needed

namespace cloth {

void HandleInput(AppState& state, float deltaTime) {
    // 1. Camera movement
    float speed = 50.0f;
    if (Input::IsKeyPressed(GLFW_KEY_LEFT_SHIFT)) speed *= 4.0f;

    if (Input::IsKeyPressed(GLFW_KEY_W)) state.camera.MoveForward(deltaTime * speed);
    if (Input::IsKeyPressed(GLFW_KEY_S)) state.camera.MoveForward(-deltaTime * speed);
    if (Input::IsKeyPressed(GLFW_KEY_A)) state.camera.MoveRight(-deltaTime * speed);
    if (Input::IsKeyPressed(GLFW_KEY_D)) state.camera.MoveRight(deltaTime * speed);
    if (Input::IsKeyPressed(GLFW_KEY_E)) state.camera.MoveUp(deltaTime * speed);
    if (Input::IsKeyPressed(GLFW_KEY_Q)) state.camera.MoveUp(-deltaTime * speed);

    // 2. Mouse rotation
    double mouseX = Input::GetMouseX();
    double mouseY = Input::GetMouseY();
    if (Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        if (state.mousePressed) {
            float deltaX = static_cast<float>(mouseX - state.lastMouseX);
            float deltaY = static_cast<float>(mouseY - state.lastMouseY);
            state.camera.Rotate(deltaX * 0.15f, -deltaY * 0.15f);
        }
        state.mousePressed = true;
    } else {
        state.mousePressed = false;
    }
    state.lastMouseX = mouseX;
    state.lastMouseY = mouseY;

    // 3. Reset cloth (Space)
    if (Input::IsKeyPressed(GLFW_KEY_SPACE)) {
        for (auto* cloth : state.cloths) {
            cloth->Reset();
        }
        // No need to update reflection - cloth movement is already captured
    }

    // 4. Terrain Texture Switching (T / Shift + T)
    bool tDown = Input::IsKeyPressed(GLFW_KEY_T);
    bool shiftDown = Input::IsKeyPressed(GLFW_KEY_LEFT_SHIFT) || Input::IsKeyPressed(GLFW_KEY_RIGHT_SHIFT);

    if (tDown && !state.lastTKeyPressed) {
        if (shiftDown) {
            state.world.PreviousTexture();
        } else {
            state.world.NextTexture();
        }
        state.reflectionNeedsUpdate = true; // Terrain changed, need to update reflections
    }
    state.lastTKeyPressed = tDown;

    // 5. Direct Texture Selection (1-9)
    for (int i = 0; i < 9; ++i) {
        int key = GLFW_KEY_1 + i;
        bool keyDown = Input::IsKeyPressed(key);
        if (keyDown && !state.lastNumberKeysPressed[i]) {
            state.world.SetCurrentTexture(i);
            state.reflectionNeedsUpdate = true;
        }
        state.lastNumberKeysPressed[i] = keyDown;
    }

    // 6. Toggle Wireframe (F)
    bool fDown = Input::IsKeyPressed(GLFW_KEY_F);
    if (fDown && !state.lastFKeyPressed) {
        state.world.GetTerrain().SetWireframe(!state.world.GetTerrain().IsWireframe());
    }
    state.lastFKeyPressed = fDown;
}

} // namespace cloth