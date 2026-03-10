#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <windows.h>

#include <glad/glad.h>
#include <stb_image.h>

#include "core/Application.h"
#include "core/Input.h"
#include "renderer/Camera.h"
#include "renderer/Shader.h"
#include "renderer/Renderer.h"
#include "physics/PhysicsWorld.h"
#include "cloth/Cloth.h"
#include "cloth/ClothMesh.h"
#include "world/World.h"

using namespace cloth;
namespace fs = std::filesystem;

// Global application pointer
static Application* g_App = nullptr;

// Helper to get absolute path relative to executable
std::string GetAssetPath(const std::string& relativePath) {
    // Try current directory first
    if (fs::exists(relativePath)) {
        return relativePath;
    }
    
    // Try relative to executable
    char pathBuf[4096];
    GetModuleFileNameA(NULL, pathBuf, sizeof(pathBuf));
    fs::path exePath = fs::path(pathBuf).parent_path();
    fs::path assetPath = exePath / relativePath;
    
    if (fs::exists(assetPath)) {
        return assetPath.string();
    }
    
    // Try relative to project root (parent of exe directory)
    fs::path projectPath = exePath.parent_path().parent_path();
    assetPath = projectPath / relativePath;
    
    if (fs::exists(assetPath)) {
        return assetPath.string();
    }
    
    return relativePath; // fallback
}

int main() {
    std::cout << "=== Cloth Simulation ===" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  WASD/Arrows - Move camera" << std::endl;
    std::cout << "  Mouse - Rotate camera (click and drag)" << std::endl;
    std::cout << "  Space - Reset cloth" << std::endl;
    std::cout << "  T/Shift+T - Change terrain texture" << std::endl;
    std::cout << "  1-4 - Select terrain texture directly" << std::endl;
    std::cout << "  Escape - Exit" << std::endl;
    std::cout << std::endl;
    std::cout << "Tip: Click and drag mouse to rotate camera and look down at the terrain!" << std::endl;
    std::cout << std::endl;

    // Create application
    Application app;
    g_App = &app;

    // Create camera - positioned higher and looking down at terrain
    Camera camera(ProjectionType::Perspective);
    camera.SetPosition(glm::vec3(0.0f, 5.0f, 10.0f));
    camera.SetRotation(glm::vec3(0.0f, -30.0f, 0.0f)); // Look down at terrain

    // Create physics world
    PhysicsWorld physicsWorld;
    physicsWorld.SetGravity(glm::vec3(0.0f, -9.81f, 0.0f));
    physicsWorld.SetIterations(5);

    // Create cloth
    ClothConfig clothConfig;
    clothConfig.widthSegments = 30;
    clothConfig.heightSegments = 20;
    clothConfig.segmentLength = 0.15f;
    clothConfig.startX = -2.25f;
    clothConfig.startY = 4.0f;
    clothConfig.startZ = 0.0f;
    clothConfig.pinTopLeft = true;
    clothConfig.pinTopRight = true;

    Cloth cloth(physicsWorld, clothConfig);
    ClothMesh clothMesh(cloth);

    // Create shaders BEFORE world (so GL context is ready for texture loading)
    Shader clothShader(
        "#version 460 core\n"
        "layout (location = 0) in vec3 a_Position;\n"
        "layout (location = 1) in vec3 a_Normal;\n"
        "layout (location = 2) in vec2 a_TexCoord;\n"
        "out vec3 v_Normal;\n"
        "out vec3 v_FragPos;\n"
        "out vec2 v_TexCoord;\n"
        "uniform mat4 u_Model;\n"
        "uniform mat4 u_View;\n"
        "uniform mat4 u_Projection;\n"
        "void main() {\n"
        "    v_FragPos = vec3(u_Model * vec4(a_Position, 1.0));\n"
        "    v_Normal = mat3(transpose(inverse(u_Model))) * a_Normal;\n"
        "    v_TexCoord = a_TexCoord;\n"
        "    gl_Position = u_Projection * u_View * u_Model * vec4(a_Position, 1.0);\n"
        "}\0",
        "#version 460 core\n"
        "in vec3 v_Normal;\n"
        "in vec3 v_FragPos;\n"
        "in vec2 v_TexCoord;\n"
        "out vec4 out_Color;\n"
        "uniform vec3 u_Color;\n"
        "uniform vec3 u_LightPos;\n"
        "uniform vec3 u_ViewPos;\n"
        "void main() {\n"
        "    float ambientStrength = 0.3;\n"
        "    vec3 ambient = ambientStrength * vec3(0.3, 0.3, 0.35);\n"
        "    vec3 norm = normalize(v_Normal);\n"
        "    vec3 lightDir = normalize(u_LightPos - v_FragPos);\n"
        "    float diff = max(dot(norm, lightDir), 0.0);\n"
        "    vec3 diffuse = diff * vec3(1.0, 0.95, 0.9);\n"
        "    float specularStrength = 0.3;\n"
        "    vec3 viewDir = normalize(u_ViewPos - v_FragPos);\n"
        "    vec3 reflectDir = reflect(-lightDir, norm);\n"
        "    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);\n"
        "    vec3 specular = specularStrength * vec3(1.0, 1.0, 1.0) * spec;\n"
        "    vec3 colorVariation = u_Color * (0.8 + 0.2 * v_Normal.y);\n"
        "    vec3 result = (ambient + diffuse + specular) * colorVariation;\n"
        "    out_Color = vec4(result, 1.0);\n"
        "}\0"
    );

    // Load terrain shader from files
    Shader terrainShader = Shader::CreateFromFile(
        GetAssetPath("shaders/terrain/terrain.vert"),
        GetAssetPath("shaders/terrain/terrain.frag")
    );

    // NOW create world (terrain) - GL context is definitely ready now
    World world;
    world.Initialize(GetAssetPath("assets/textures/lands/"));

    // Camera control variables
    bool mousePressed = false;
    double lastMouseX = 0;
    double lastMouseY = 0;
    float cameraYaw = 0.0f;
    float cameraPitch = 0.0f;

    // Set update callback
    app.SetUpdateCallback([&](float deltaTime) {
        // Camera controls
        float cameraSpeed = 5.0f * deltaTime;

        if (Input::IsKeyPressed(GLFW_KEY_W) || Input::IsKeyPressed(GLFW_KEY_UP)) {
            camera.MoveForward(cameraSpeed);
        }
        if (Input::IsKeyPressed(GLFW_KEY_S) || Input::IsKeyPressed(GLFW_KEY_DOWN)) {
            camera.MoveForward(-cameraSpeed);
        }
        if (Input::IsKeyPressed(GLFW_KEY_A) || Input::IsKeyPressed(GLFW_KEY_LEFT)) {
            camera.MoveRight(-cameraSpeed);
        }
        if (Input::IsKeyPressed(GLFW_KEY_D) || Input::IsKeyPressed(GLFW_KEY_RIGHT)) {
            camera.MoveRight(cameraSpeed);
        }

        // Mouse look
        if (Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) && !mousePressed) {
            mousePressed = true;
            lastMouseX = Input::GetMouseX();
            lastMouseY = Input::GetMouseY();
        }
        if (!Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
            mousePressed = false;
        }
        if (mousePressed) {
            double currentX = Input::GetMouseX();
            double currentY = Input::GetMouseY();

            float deltaX = static_cast<float>(currentX - lastMouseX);
            float deltaY = static_cast<float>(currentY - lastMouseY);

            cameraYaw += deltaX * 0.1f;
            cameraPitch -= deltaY * 0.1f;

            if (cameraPitch > 89.0f) cameraPitch = 89.0f;
            if (cameraPitch < -89.0f) cameraPitch = -89.0f;

            camera.SetRotation(glm::vec3(cameraYaw, cameraPitch, 0.0f));

            lastMouseX = currentX;
            lastMouseY = currentY;
        }

        // Reset cloth
        if (Input::IsKeyPressed(GLFW_KEY_SPACE)) {
            cloth.Reset();
        }

        // Terrain texture selection
        if (Input::IsKeyPressed(GLFW_KEY_T)) {
            if (Input::IsKeyPressed(GLFW_KEY_LEFT_SHIFT) || Input::IsKeyPressed(GLFW_KEY_RIGHT_SHIFT)) {
                world.PreviousTexture();
            } else {
                world.NextTexture();
            }
            // Small delay to prevent rapid cycling
            while (Input::IsKeyPressed(GLFW_KEY_T)) {
                Window::PollEvents();
            }
        }

        // Direct texture selection with number keys
        int textureCount = world.GetTextureCount();
        for (int i = 0; i < textureCount && i < 9; ++i) {
            if (Input::IsKeyPressed(GLFW_KEY_1 + i)) {
                world.SetCurrentTexture(i);
                while (Input::IsKeyPressed(GLFW_KEY_1 + i)) {
                    Window::PollEvents();
                }
                break;
            }
        }

        // Exit
        if (Input::IsKeyPressed(GLFW_KEY_ESCAPE)) {
            app.Quit();
        }

        // Update physics
        physicsWorld.Update(deltaTime);

        // Update cloth mesh
        clothMesh.Update();
    });

    // Set render callback
    app.SetRenderCallback([&]() {
        // Load texture DIRECTLY in render callback on first frame
        static bool textureLoaded = false;
        static unsigned int directTextureID = 0;
        static int directVAO = 0;
        static int directIndexCount = 0;
        static int currentTextureIndex = -1;
        
        // Check if texture needs to be (re)loaded (first time or texture changed)
        int newTextureIndex = world.GetCurrentTextureIndex();
        bool needReload = !textureLoaded || (newTextureIndex != currentTextureIndex);
        
        if (needReload && newTextureIndex >= 0) {
            if (!textureLoaded) {
                std::cout << "=== Creating texture DIRECTLY in render callback ===" << std::endl;
            } else {
                std::cout << "=== Switching to texture index " << newTextureIndex << " ===" << std::endl;
            }
            
            // Delete old texture if exists
            if (textureLoaded && glIsTexture(directTextureID)) {
                glDeleteTextures(1, &directTextureID);
            }
            
            // Create texture directly here
            glGenTextures(1, &directTextureID);
            std::cout << "glGenTextures: " << directTextureID << ", glIsTexture = " << (int)glIsTexture(directTextureID) << std::endl;
            
            glBindTexture(GL_TEXTURE_2D, directTextureID);
            std::cout << "After glBindTexture: glIsTexture = " << (int)glIsTexture(directTextureID) << std::endl;
            
            // Get texture path from world
            const auto& texturePaths = world.GetTexturePaths();
            std::string texturePath = texturePaths[newTextureIndex];
            std::cout << "Loading texture: " << texturePath << std::endl;
            
            // Load image data using existing stb_image functions from Texture.cpp
            stbi_set_flip_vertically_on_load(1);
            int width, height, channels;
            unsigned char* data = stbi_load(texturePath.c_str(), &width, &height, &channels, 0);
            
            if (data) {
                GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
                glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
                glGenerateMipmap(GL_TEXTURE_2D);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                stbi_image_free(data);
                std::cout << "Texture loaded: " << width << "x" << height << std::endl;
            } else {
                std::cerr << "Failed to load texture: " << texturePath << std::endl;
            }
            
            std::cout << "Final glIsTexture(" << directTextureID << ") = " << (int)glIsTexture(directTextureID) << std::endl;
            std::cout << "=== Texture creation complete ===" << std::endl;
            
            // Get VAO info from terrain
            directVAO = world.GetTerrain().GetVAO();
            directIndexCount = world.GetTerrain().GetIndexCount();
            
            currentTextureIndex = newTextureIndex;
            textureLoaded = true;
        }
        
        // Update camera projection
        camera.SetProjection(static_cast<float>(app.GetWindow().GetWidth()) /
                            static_cast<float>(app.GetWindow().GetHeight()));

        // Setup model matrix
        glm::mat4 model = glm::mat4(1.0f);

        // === Draw terrain with DIRECT texture ===
        glDisable(GL_CULL_FACE);
        
        if (textureLoaded && glIsTexture(directTextureID)) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, directTextureID);
            
            terrainShader.Bind();
            terrainShader.SetMat4("u_Model", model);
            terrainShader.SetMat4("u_View", camera.GetViewMatrix());
            terrainShader.SetMat4("u_Projection", camera.GetProjectionMatrix());
            terrainShader.SetInt("u_TerrainTexture", 0);
            
            glBindVertexArray(directVAO);
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
            glEnableVertexAttribArray(2);
            
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(directIndexCount),
                           GL_UNSIGNED_INT, 0);
            
            glBindVertexArray(0);
            terrainShader.Unbind();
        }
        
        glEnable(GL_CULL_FACE);

        // === Draw cloth AFTER terrain ===
        clothShader.Bind();
        clothShader.SetMat4("u_Model", model);
        clothShader.SetMat4("u_View", camera.GetViewMatrix());
        clothShader.SetMat4("u_Projection", camera.GetProjectionMatrix());
        clothShader.SetVec3("u_Color", glm::vec3(0.2f, 0.5f, 0.8f));
        clothShader.SetVec3("u_LightPos", glm::vec3(5.0f, 10.0f, 5.0f));
        clothShader.SetVec3("u_ViewPos", camera.GetPosition());
        clothMesh.Draw(*reinterpret_cast<Renderer*>(nullptr), clothShader);
        clothShader.Unbind();
    });

    // Run application
    app.Run();

    return 0;
}
