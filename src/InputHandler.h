#pragma once

#include "AppState.h"
#include "core/Input.h"

namespace cloth {
    // Handle all input and update game state
    void HandleInput(AppState& state, float deltaTime);
}
