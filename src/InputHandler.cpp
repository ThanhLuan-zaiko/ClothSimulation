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
    if (Input::IsKeyPressed(GLFW_KEY_LEFT_CONTROL)) state.camera.MoveUp(deltaTime * speed);
    if (Input::IsKeyPressed(GLFW_KEY_Q)) state.camera.MoveUp(-deltaTime * speed);

    // 2. Mouse rotation and Cloth Interaction
    double mouseX = Input::GetMouseX();
    double mouseY = Input::GetMouseY();
    
    // Check for Interaction mode (E key)
    state.isInteracting = Input::IsKeyPressed(GLFW_KEY_E);

    if (state.isInteracting) {
        // Calculate interaction world position
        // Get viewport for unproject
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        
        // Calculate ray in world space
        glm::vec3 win(mouseX, (float)viewport[3] - mouseY, 0.0f); // Near plane
        glm::vec3 winFar(mouseX, (float)viewport[3] - mouseY, 1.0f); // Far plane
        glm::mat4 view = state.camera.GetViewMatrix();
        glm::mat4 proj = state.camera.GetProjectionMatrix();

        glm::vec3 worldNear = glm::unProject(win, view, proj, glm::vec4(0, 0, viewport[2], viewport[3]));
        glm::vec3 worldFar = glm::unProject(winFar, view, proj, glm::vec4(0, 0, viewport[2], viewport[3]));

        glm::vec3 rayDir = glm::normalize(worldFar - worldNear);
        glm::vec3 rayOrigin = state.camera.GetPosition();

        // ACCURATE 3D PICKING: Read depth buffer at mouse position
        float depth = 1.0f;
        glReadPixels((int)mouseX, viewport[3] - (int)mouseY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);

        float targetDistance = 0.0f;
        if (depth < 1.0f) {
            // If we hit SOMETHING (cloth, sphere, or ground), use that exact depth
            glm::vec3 win(mouseX, (float)viewport[3] - mouseY, depth);
            state.interactionWorldPos = glm::unProject(win, view, proj, glm::vec4(0, 0, viewport[2], viewport[3]));
        } else {
            // FALLBACK: If pointing at sky, use previous logic
            if (rayDir.y < -0.05f) {
                targetDistance = -rayOrigin.y / rayDir.y;
                targetDistance = glm::clamp(targetDistance, 1.0f, 100.0f);
            } else {
                targetDistance = glm::distance(rayOrigin, state.mirrorSphere.GetPosition());
            }
            state.interactionWorldPos = rayOrigin + rayDir * targetDistance;
        }

        // Visual feedback
    } else if (Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        // Only rotate camera if NOT interacting
        // Calculate delta from last frame
        float deltaX = static_cast<float>(mouseX - state.lastMouseX);
        float deltaY = static_cast<float>(mouseY - state.lastMouseY);
        
        // Rotate camera based on mouse movement
        state.camera.Rotate(deltaX * 0.15f, -deltaY * 0.15f);
        state.mousePressed = true;
    } else {
        state.mousePressed = false;
    }
    
    // Update last mouse position
    state.lastMouseX = mouseX;
    state.lastMouseY = mouseY;

    // 3. Reset cloth (Space)
    bool spaceDown = Input::IsKeyPressed(GLFW_KEY_SPACE);
    if (spaceDown && !state.lastSpaceKeyPressed) {
        for (size_t i = 0; i < state.cloths.size(); i++) {
            Cloth* cloth = state.cloths[i];
            size_t offset = state.clothParticleOffsets[i];
            
            // Get cloth config for initial positions using getters
            int w = cloth->GetWidthSegments();
            int h = cloth->GetHeightSegments();
            float segmentLen = cloth->GetSegmentLength();
            float startX = cloth->GetStartX();
            float startY = cloth->GetStartY();
            float startZ = cloth->GetStartZ();

            // Reset GPU buffers
            state.physicsWorld.ResetCloth(offset, w, h, startX, startY, startZ, segmentLen, true, (int)i);
            
            // Reset drop state in AppState
            state.clothDropTimers[i] = 0.0f;
            state.clothDropped[i] = false;
        }
    }
    state.lastSpaceKeyPressed = spaceDown;

    // 4. Reset Camera (H)
    bool hDown = Input::IsKeyPressed(GLFW_KEY_H);
    if (hDown && !state.lastHKeyPressed) {
        state.camera.SetPosition(glm::vec3(0.0f, 15.0f, 25.0f));
        state.cameraYaw = -90.0f;
        state.cameraPitch = -20.0f;
        state.camera.SetRotation(glm::vec3(state.cameraYaw, state.cameraPitch, 0.0f));
    }
    state.lastHKeyPressed = hDown;

    // 5. Terrain Texture Switching (T / Shift + T)
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
    state.lastEKeyPressed = state.isInteracting;

    // ========================================================================
    // DEBUG VISUALIZATION CONTROLS
    // ========================================================================
    
    // INSERT: Toggle debug visualization on/off (replaced F12 to avoid VS conflict)
    bool insertDown = Input::IsKeyPressed(GLFW_KEY_INSERT);
    if (insertDown && !state.lastInsertKeyPressed) {
        state.physicsWorld.ToggleDebugMode();
    }
    state.lastInsertKeyPressed = insertDown;

    // DELETE: Cycle through debug modes (replaced F11 to avoid VS conflict)
    bool deleteDown = Input::IsKeyPressed(GLFW_KEY_DELETE);
    if (deleteDown && !state.lastDeleteKeyPressed) {
        state.physicsWorld.CycleDebugMode();
    }
    state.lastDeleteKeyPressed = deleteDown;

    // F1-F10: Set specific debug modes
    for (int i = 0; i < 10; ++i) {
        int key = GLFW_KEY_F1 + i;
        bool keyDown = Input::IsKeyPressed(key);
        if (keyDown && !state.lastFKeysPressed[i]) {
            SetSpecificDebugMode(state, i);
        }
        state.lastFKeysPressed[i] = keyDown;
    }
}

void ToggleDebugMode(AppState& state) {
    state.physicsWorld.ToggleDebugMode();
}

void CycleDebugMode(AppState& state) {
    state.physicsWorld.CycleDebugMode();
}

void SetSpecificDebugMode(AppState& state, int mode) {
    auto debugMode = static_cast<GPUPhysicsWorld::DebugMode>(mode);
    state.physicsWorld.SetDebugMode(debugMode);
}

} // namespace cloth