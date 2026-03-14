#pragma once

#include "Terrain.h"
#include "Skybox.h"
#include "WorldObject.h"
#include <string>
#include <vector>
#include <memory>

namespace cloth {

class World {
public:
    World();
    ~World();

    void Initialize(const std::string& textureDir = "assets/textures/lands/");
    void InitializeGL();  // Call after GL context is ready to create terrain mesh
    void LoadTextures();  // Call this when GL context is ready

    void SetCurrentTexture(int index);
    void NextTexture();
    void PreviousTexture();
    void CycleTexture(int direction); // direction: 1 for next, -1 for previous

    bool IsTexturesLoaded() const { return m_TexturesLoaded; }

    Terrain& GetTerrain() { return m_Terrain; }
    const Terrain& GetTerrain() const { return m_Terrain; }

    int GetCurrentTextureIndex() const { return m_CurrentTextureIndex; }
    int GetTextureCount() const { return static_cast<int>(m_TexturePaths.size()); }
    const std::string& GetCurrentTextureName() const;
    const std::vector<std::string>& GetTexturePaths() const { return m_TexturePaths; }

    Skybox& GetSkybox() { return m_Skybox; }
    const Skybox& GetSkybox() const { return m_Skybox; }

    // World objects management
    void AddObject(std::shared_ptr<WorldObject> obj);
    const std::vector<std::shared_ptr<WorldObject>>& GetObjects() const { return m_Objects; }
    
    // Helper methods to add objects
    Pillar* AddPillar(const glm::vec3& position, float height = 5.0f, float radius = 0.3f);
    Rock* AddRock(const glm::vec3& position, float size = 1.0f, float irregularity = 0.3f);

private:
    void ScanTextureDirectory(const std::string& textureDir);

    Terrain m_Terrain;
    Skybox m_Skybox;
    std::vector<std::shared_ptr<WorldObject>> m_Objects;
    std::vector<std::string> m_TexturePaths;
    std::vector<std::string> m_TextureNames;
    int m_CurrentTextureIndex;
    bool m_TexturesLoaded;
};

} // namespace cloth
