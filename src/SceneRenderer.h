#pragma once

#include "AppState.h"
#include "core/Application.h"
#include "renderer/Renderer.h"

namespace cloth {
    // Render the entire scene
    void Render(AppState& state, const Application& app);
}
