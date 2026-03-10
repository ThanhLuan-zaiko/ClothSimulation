#pragma once

#include "Terrain.h"
#include <string>
#include <vector>

namespace cloth {

class World {
public:
    World();
    ~World();

    void Initialize(const std::string& textureDir = "assets/textures/lands/");
    void LoadTextures();  // Call this when GL context is ready

    void SetCurrentTexture(int index);
    void NextTexture();
    void PreviousTexture();
    void CycleTexture(int direction); // direction: 1 for next, -1 for previous

    Terrain& GetTerrain() { return m_Terrain; }
    const Terrain& GetTerrain() const { return m_Terrain; }

    int GetCurrentTextureIndex() const { return m_CurrentTextureIndex; }
    int GetTextureCount() const { return static_cast<int>(m_TexturePaths.size()); }
    const std::string& GetCurrentTextureName() const;
    const std::vector<std::string>& GetTexturePaths() const { return m_TexturePaths; }

private:
    void ScanTextureDirectory(const std::string& textureDir);

    Terrain m_Terrain;
    std::vector<std::string> m_TexturePaths;
    std::vector<std::string> m_TextureNames;
    int m_CurrentTextureIndex;
    bool m_TexturesLoaded;
};

} // namespace cloth
