#pragma once

#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "renderer/Camera.h"
#include "renderer/Shader.h"
#include "renderer/ReflectionCubemap.h"
#include "physics/PhysicsWorld.h"
#include "cloth/Cloth.h"
#include "cloth/ClothMesh.h"
#include "world/World.h"
#include "world/Sphere.h"

using namespace cloth;

namespace cloth {

// Global application state
struct AppState {
    // Camera
    Camera camera;
    float cameraYaw = -90.0f;
    float cameraPitch = -20.0f;
    float cameraFOV = 45.0f;

    // Physics
    PhysicsWorld physicsWorld;

    // Cloths
    std::vector<Cloth*> cloths;
    std::vector<ClothMesh*> clothMeshes;
    std::vector<unsigned int> clothTextures;
    
    // Cloth drop animation
    std::vector<float> clothDropTimers;      // Timer for each cloth drop
    std::vector<bool> clothDropped;          // Whether each cloth has started falling
    float clothDropDelay = 1.5f;             // Delay between each cloth drop
    float clothDropStartDelay = 0.5f;        // Initial delay before first cloth drops

    // World and sphere
    World world;
    Sphere mirrorSphere;

    // Shaders
    Shader clothShader;
    Shader terrainShader;
    Shader sphereShader;
    Shader skyboxShader;

    // Reflection
    ReflectionCubemap* reflectionCubemap = nullptr;  // Use pointer - initialized in InitializeGL()
    bool reflectionNeedsUpdate = true;
    
    // Lazy update tracking
    glm::vec3 lastReflectionUpdatePos;  // Last camera position when reflection was updated
    float reflectionUpdateThreshold = 2.5f;  // Min distance to trigger update (increased for better performance)
    int lastTerrainTextureIndex = -1;  // Track terrain texture index

    // Terrain texture state
    unsigned int terrainTextureID = 0;
    int terrainVAO = 0;
    int terrainIndexCount = 0;
    int currentTerrainTextureIndex = -1;
    bool terrainTextureLoaded = false;

    // Cloth textures state
    bool clothTexturesLoaded = false;

    // Mouse state
    bool mousePressed = false;
    double lastMouseX = 0;
    double lastMouseY = 0;

    // Key repeat tracking
    float keyRepeatTimer = 0.0f;
    float keyRepeatDelay = 0.15f;
    bool lastTKeyPressed = false;
    bool lastShiftKeyPressed = false;
    bool lastFKeyPressed = false;
    bool lastNumberKeysPressed[9] = {false};

    // Constructor
    AppState()
        : camera(ProjectionType::Perspective)
        , clothShader(
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
            "uniform sampler2D u_ClothTexture;\n"
            "uniform bool u_UseTexture;\n"
            "void main() {\n"
            "    float ambientStrength = 0.3;\n"
            "    vec3 ambient = ambientStrength * vec3(0.3, 0.3, 0.35);\n"
            "    vec3 norm = normalize(v_Normal);\n"
            "    vec3 lightDir = normalize(u_LightPos - v_FragPos);\n"
            "    float diff = max(dot(norm, lightDir), 0.0);\n"
            "    vec3 diffuse = diff * vec3(1.0, 0.95, 0.9);\n"
            "    float specularStrength = 0.2;\n"
            "    vec3 viewDir = normalize(u_ViewPos - v_FragPos);\n"
            "    vec3 reflectDir = reflect(-lightDir, norm);\n"
            "    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);\n"
            "    vec3 specular = specularStrength * vec3(1.0, 1.0, 1.0) * spec;\n"
            "    vec3 baseColor;\n"
            "    if (u_UseTexture) {\n"
            "        baseColor = texture(u_ClothTexture, v_TexCoord).rgb;\n"
            "    } else {\n"
            "        vec3 colorVariation = u_Color * (0.8 + 0.2 * v_Normal.y);\n"
            "        baseColor = colorVariation;\n"
            "    }\n"
            "    vec3 result = (ambient + diffuse + specular) * baseColor;\n"
            "    out_Color = vec4(result, 1.0);\n"
            "}\0"
        )
    {
        // Initialize camera position - looking at the scene from a distance
        camera.SetPosition(glm::vec3(0.0f, 15.0f, 25.0f));
        camera.SetRotation(glm::vec3(-90.0f, -20.0f, 0.0f));

        // Initialize physics world
        physicsWorld.SetGravity(glm::vec3(0.0f, -9.81f, 0.0f));
        physicsWorld.SetIterations(5);
    }

    // Initialize OpenGL-dependent objects (call after GL context is ready)
    void InitializeGL() {
        // Gigantic 3m radius mirror sphere
        mirrorSphere.SetRadius(3.0f);
        mirrorSphere.Initialize();
        mirrorSphere.SetPosition(glm::vec3(0.0f, 10.0f, 0.0f)); // Raised above ground for better visibility
        mirrorSphere.SetColor(glm::vec3(1.0f, 1.0f, 1.0f));

        // Initialize reflection cubemap (AFTER GL context is ready!)
        // 512x512 is a good balance between quality and performance
        reflectionCubemap = new ReflectionCubemap(512);

        // Initialize lazy update tracking
        lastReflectionUpdatePos = camera.GetPosition();
        lastTerrainTextureIndex = -1;

        // Set collision sphere in physics world
        physicsWorld.SetCollisionSphere(mirrorSphere.GetPosition(), mirrorSphere.GetRadius());
    }

    // Destructor - clean up dynamically allocated resources
    ~AppState() {
        // Clean up reflection cubemap
        if (reflectionCubemap) {
            delete reflectionCubemap;
            reflectionCubemap = nullptr;
        }
        
        // Clean up cloth textures
        for (unsigned int texID : clothTextures) {
            if (glIsTexture(texID)) {
                glDeleteTextures(1, &texID);
            }
        }

        // Clean up terrain texture
        if (terrainTextureLoaded && glIsTexture(terrainTextureID)) {
            glDeleteTextures(1, &terrainTextureID);
        }

        // Clean up cloths and cloth meshes
        for (auto* mesh : clothMeshes) {
            delete mesh;
        }
        for (auto* c : cloths) {
            delete c;
        }
    }
};

} // namespace cloth
