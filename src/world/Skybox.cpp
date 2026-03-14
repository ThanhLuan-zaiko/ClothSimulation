#include "Skybox.h"
#include <glad/glad.h>
#include <stb_image.h>
#include <iostream>
#include <cmath>
#include <filesystem>
#include <vector>
#include <glm/gtc/constants.hpp>

namespace cloth {

Skybox::Skybox()
    : m_VAO(0), m_VBO(0), m_CubemapTexture(0), m_Initialized(false) {
}

Skybox::~Skybox() {
    if (m_VAO != 0) {
        glDeleteVertexArrays(1, &m_VAO);
        glDeleteBuffers(1, &m_VBO);
    }
    if (m_CubemapTexture != 0) {
        glDeleteTextures(1, &m_CubemapTexture);
    }
}

void Skybox::Initialize() {
    if (m_Initialized) return;
    
    SetupBuffers();
    m_Initialized = true;
    
    std::cout << "Skybox initialized" << std::endl;
}

void Skybox::SetupBuffers() {
    // Create a large sphere for skybox instead of cube
    const int rings = 32;
    const int sectors = 64;
    const float radius = 50.0f;
    
    std::vector<float> vertices;
    
    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < sectors; s++) {
            float y1 = cos(float(r) / rings * 3.14159f);
            float x1 = sin(float(r) / rings * 3.14159f) * cos(float(s) / sectors * 2.0f * 3.14159f);
            float z1 = sin(float(r) / rings * 3.14159f) * sin(float(s) / sectors * 2.0f * 3.14159f);
            
            float y2 = cos(float(r + 1) / rings * 3.14159f);
            float x2 = sin(float(r + 1) / rings * 3.14159f) * cos(float(s) / sectors * 2.0f * 3.14159f);
            float z2 = sin(float(r + 1) / rings * 3.14159f) * sin(float(s) / sectors * 2.0f * 3.14159f);
            
            float y3 = cos(float(r + 1) / rings * 3.14159f);
            float x3 = sin(float(r + 1) / rings * 3.14159f) * cos(float(s + 1) / sectors * 2.0f * 3.14159f);
            float z3 = sin(float(r + 1) / rings * 3.14159f) * sin(float(s + 1) / sectors * 2.0f * 3.14159f);
            
            float y4 = cos(float(r) / rings * 3.14159f);
            float x4 = sin(float(r) / rings * 3.14159f) * cos(float(s + 1) / sectors * 2.0f * 3.14159f);
            float z4 = sin(float(r) / rings * 3.14159f) * sin(float(s + 1) / sectors * 2.0f * 3.14159f);
            
            // First triangle
            vertices.push_back(x1 * radius); vertices.push_back(y1 * radius); vertices.push_back(z1 * radius);
            vertices.push_back(x3 * radius); vertices.push_back(y3 * radius); vertices.push_back(z3 * radius);
            vertices.push_back(x2 * radius); vertices.push_back(y2 * radius); vertices.push_back(z2 * radius);
            
            // Second triangle
            vertices.push_back(x1 * radius); vertices.push_back(y1 * radius); vertices.push_back(z1 * radius);
            vertices.push_back(x4 * radius); vertices.push_back(y4 * radius); vertices.push_back(z4 * radius);
            vertices.push_back(x3 * radius); vertices.push_back(y3 * radius); vertices.push_back(z3 * radius);
        }
    }

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);
    
    std::cout << "Skybox sphere created with " << vertices.size() / 3 << " vertices" << std::endl;
}

void Skybox::LoadTextures(const std::string& folderPath) {
    std::vector<std::string> faces;
    faces.reserve(6);
    
    // Try common skybox texture naming conventions
    std::vector<std::string> suffixes = {
        "right", "left", "top", "bottom", "front", "back",
        "px", "nx", "py", "ny", "pz", "nz",
        "0", "1", "2", "3", "4", "5"
    };
    
    // Try to find skybox textures
    std::vector<std::string> possibleNames = {
        folderPath + "/skybox_right.jpg",
        folderPath + "/skybox_left.jpg",
        folderPath + "/skybox_top.jpg",
        folderPath + "/skybox_bottom.jpg",
        folderPath + "/skybox_front.jpg",
        folderPath + "/skybox_back.jpg"
    };
    
    // Check if files exist, if not use procedural skybox color
    bool texturesFound = true;
    for (const auto& path : possibleNames) {
        if (!std::filesystem::exists(path)) {
            texturesFound = false;
            break;
        }
    }
    
    if (texturesFound) {
        faces = possibleNames;
        LoadCubemap(faces);
        std::cout << "Skybox textures loaded from: " << folderPath << std::endl;
    } else {
        std::cout << "Skybox textures not found, using gradient sky" << std::endl;
        // Create a simple gradient sky texture
        CreateGradientSky();
    }
}

void Skybox::CreateGradientSky() {
    // Create cubemap with DISTINCT colors for each face (for testing)
    glGenTextures(1, &m_CubemapTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_CubemapTexture);

    int width = 512;
    int height = 512;

    // Generate each face with a consistent Sky Blue color
    for (int face = 0; face < 6; face++) {
        std::vector<unsigned char> pixels(width * height * 3);

        float r, g, b;
        if (face == 2) { // TOP
            r = 0.5f; g = 0.7f; b = 1.0f; 
        } else if (face == 3) { // BOTTOM
            r = 0.7f; g = 0.8f; b = 0.9f;
        } else { // SIDES
            r = 0.6f; g = 0.75f; b = 1.0f;
        }

        // Fill all pixels with the selected blue
        for (int i = 0; i < width * height; i++) {
            pixels[i * 3] = static_cast<unsigned char>(r * 255.0f);
            pixels[i * 3 + 1] = static_cast<unsigned char>(g * 255.0f);
            pixels[i * 3 + 2] = static_cast<unsigned char>(b * 255.0f);
        }

        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

void Skybox::LoadCubemap(const std::vector<std::string>& faces) {
    glGenTextures(1, &m_CubemapTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_CubemapTexture);

    stbi_set_flip_vertically_on_load(false);
    
    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++) {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
            std::cout << "Loaded skybox face " << i << ": " << faces[i] << std::endl;
        } else {
            std::cerr << "Failed to load skybox face " << i << ": " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

void Skybox::Draw() const {
    if (!m_Initialized) return;
    
    glDepthFunc(GL_LEQUAL);  // Change depth function so skybox passes depth test
    
    glBindVertexArray(m_VAO);
    // Draw all vertices generated in SetupBuffers
    // 32 rings * 64 sectors * 2 triangles * 3 vertices = 12288 vertices
    glDrawArrays(GL_TRIANGLES, 0, 12288); 
    glBindVertexArray(0);
    
    glDepthFunc(GL_LESS);  // Reset depth function
}

} // namespace cloth
