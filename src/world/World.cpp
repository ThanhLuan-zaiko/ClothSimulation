#include "World.h"
#include <algorithm>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace cloth {

World::World()
    : m_Terrain(100.0f, 100.0f, 100, 10.0f), m_CurrentTextureIndex(0), m_TexturesLoaded(false) {
}

World::~World() {
}

void World::Initialize(const std::string& textureDir) {
    ScanTextureDirectory(textureDir);

    if (m_TexturePaths.empty()) {
        std::cerr << "Warning: No textures found in " << textureDir << std::endl;
        return;
    }

    // Initialize skybox
    m_Skybox.Initialize();
    m_Skybox.LoadTextures(textureDir);

    std::cout << "World initialized with " << m_TexturePaths.size() << " textures (paths scanned)" << std::endl;
    std::cout << "Textures will be loaded on first render call" << std::endl;
}

void World::LoadTextures() {
    if (m_TexturesLoaded || m_TexturePaths.empty()) {
        return;
    }

    std::cout << "Loading textures now (GL context should be ready)..." << std::endl;
    
    // Load first texture
    SetCurrentTexture(0);
    
    m_TexturesLoaded = true;
    std::cout << "Textures loaded successfully!" << std::endl;
}

void World::ScanTextureDirectory(const std::string& textureDir) {
    m_TexturePaths.clear();
    m_TextureNames.clear();

    try {
        if (!fs::exists(textureDir)) {
            std::cerr << "Texture directory does not exist: " << textureDir << std::endl;
            return;
        }

        std::vector<std::pair<std::string, std::string>> textureFiles;

        for (const auto& entry : fs::directory_iterator(textureDir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" || ext == ".tga") {
                    textureFiles.emplace_back(entry.path().string(), entry.path().stem().string());
                }
            }
        }

        // Sort alphabetically
        std::sort(textureFiles.begin(), textureFiles.end(),
                  [](const auto& a, const auto& b) { return a.second < b.second; });

        for (const auto& [path, name] : textureFiles) {
            m_TexturePaths.push_back(path);
            m_TextureNames.push_back(name);
        }

    } catch (const std::exception& e) {
        std::cerr << "Error scanning texture directory: " << e.what() << std::endl;
    }
}

void World::SetCurrentTexture(int index) {
    if (index < 0 || index >= static_cast<int>(m_TexturePaths.size())) {
        return;
    }

    m_CurrentTextureIndex = index;
    m_Terrain.LoadTexture(m_TexturePaths[index]);

    std::cout << "Terrain texture changed to: " << m_TextureNames[index] << std::endl;
}

void World::NextTexture() {
    if (m_TexturePaths.empty()) return;

    m_CurrentTextureIndex = (m_CurrentTextureIndex + 1) % static_cast<int>(m_TexturePaths.size());
    SetCurrentTexture(m_CurrentTextureIndex);
}

void World::PreviousTexture() {
    if (m_TexturePaths.empty()) return;

    m_CurrentTextureIndex = (m_CurrentTextureIndex - 1 + static_cast<int>(m_TexturePaths.size())) %
                            static_cast<int>(m_TexturePaths.size());
    SetCurrentTexture(m_CurrentTextureIndex);
}

void World::CycleTexture(int direction) {
    if (direction > 0) {
        NextTexture();
    } else if (direction < 0) {
        PreviousTexture();
    }
}

const std::string& World::GetCurrentTextureName() const {
    static const std::string empty = "";
    if (m_CurrentTextureIndex >= 0 && m_CurrentTextureIndex < static_cast<int>(m_TextureNames.size())) {
        return m_TextureNames[m_CurrentTextureIndex];
    }
    return empty;
}

void World::AddObject(std::shared_ptr<WorldObject> obj) {
    m_Objects.push_back(obj);
}

Pillar* World::AddPillar(const glm::vec3& position, float height, float radius) {
    auto pillar = std::make_shared<Pillar>(height, radius);
    pillar->SetPosition(position);
    m_Objects.push_back(pillar);
    return pillar.get();
}

Rock* World::AddRock(const glm::vec3& position, float size, float irregularity) {
    auto rock = std::make_shared<Rock>(size, irregularity);
    rock->SetPosition(position);
    m_Objects.push_back(rock);
    return rock.get();
}

} // namespace cloth
