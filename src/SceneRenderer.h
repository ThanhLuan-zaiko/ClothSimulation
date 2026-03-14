#pragma once

#include "AppState.h"
#include "core/Application.h"

// Load cloth textures from assets
void LoadClothTextures(AppState& state);

// Load terrain texture
void LoadTerrainTexture(AppState& state);

// Render the entire scene
void Render(AppState& state, const Application& app);
