#pragma once

#include "AppState.h"
#include "core/Input.h"

namespace cloth {
    // Handle all input and update game state
    void HandleInput(AppState& state, float deltaTime);
    
    // Debug visualization handlers
    void ToggleDebugMode(AppState& state);
    void CycleDebugMode(AppState& state);
    void SetSpecificDebugMode(AppState& state, int mode);
}
