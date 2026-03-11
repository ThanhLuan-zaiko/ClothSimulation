#include "Skybox.h"
#include <glad/glad.h>
#include <stb_image.h>
#include <iostream>
#include <cmath>
#include <filesystem>

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
    float skyboxVertices[] = {
        // Positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);
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
    // Create a simple gradient cubemap
    glGenTextures(1, &m_CubemapTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_CubemapTexture);
    
    int width = 512;
    int height = 512;
    
    // Create gradient colors
    std::vector<unsigned char> pixels(width * height * 3);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float t = static_cast<float>(y) / static_cast<float>(height);
            
            // Gradient from light blue (top) to darker blue (bottom)
            unsigned char r = static_cast<unsigned char>(135.0f * (1.0f - t) + 200.0f * t);
            unsigned char g = static_cast<unsigned char>(206.0f * (1.0f - t) + 230.0f * t);
            unsigned char b = static_cast<unsigned char>(235.0f * (1.0f - t) + 255.0f * t);
            
            int idx = (y * width + x) * 3;
            pixels[idx] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
        }
    }
    
    // Set all 6 faces with the same gradient
    for (unsigned int i = 0; i < 6; i++) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    }
    
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    
    std::cout << "Gradient skybox created" << std::endl;
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
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    
    glDepthFunc(GL_LESS);  // Reset depth function
}

} // namespace cloth
