#pragma once

#include <string>
#include <glm/glm.hpp>

namespace cloth {

class Texture {
public:
    Texture();
    Texture(const std::string& path);
    ~Texture();

    // Prevent copying to avoid double-deletion of OpenGL resources
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Support moving for resource transfer
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    void Bind(unsigned int slot = 0) const;
    void Unbind() const;

    unsigned int GetID() const { return m_RendererID; }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }

    static Texture CreateFromFile(const std::string& path);

private:
    unsigned int m_RendererID;
    int m_Width;
    int m_Height;
    int m_Channels;
    std::string m_Path;

    void LoadTexture(const std::string& path);
};

} // namespace cloth
