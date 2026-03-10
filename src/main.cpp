#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/Application.h"
#include "core/Input.h"
#include "renderer/Camera.h"
#include "renderer/Shader.h"
#include "renderer/Renderer.h"
#include "physics/PhysicsWorld.h"
#include "cloth/Cloth.h"
#include "cloth/ClothMesh.h"

using namespace cloth;

// Global application pointer
static Application* g_App = nullptr;

int main() {
    std::cout << "=== Cloth Simulation ===" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  WASD/Arrows - Move camera" << std::endl;
    std::cout << "  Mouse - Rotate camera (click and drag)" << std::endl;
    std::cout << "  Space - Reset cloth" << std::endl;
    std::cout << "  Escape - Exit" << std::endl;
    std::cout << std::endl;

    // Create application
    Application app;
    g_App = &app;

    // Create camera
    Camera camera(ProjectionType::Perspective);
    camera.SetPosition(glm::vec3(0.0f, 2.0f, 8.0f));
    camera.SetRotation(glm::vec3(0.0f, 0.0f, 0.0f));

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

    // Create shader
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
        // Update camera projection
        camera.SetProjection(static_cast<float>(app.GetWindow().GetWidth()) /
                            static_cast<float>(app.GetWindow().GetHeight()));

        // Setup model matrix
        glm::mat4 model = glm::mat4(1.0f);

        // Setup shader uniforms
        clothShader.Bind();
        clothShader.SetMat4("u_Model", model);
        clothShader.SetMat4("u_View", camera.GetViewMatrix());
        clothShader.SetMat4("u_Projection", camera.GetProjectionMatrix());
        clothShader.SetVec3("u_Color", glm::vec3(0.2f, 0.5f, 0.8f));
        clothShader.SetVec3("u_LightPos", glm::vec3(5.0f, 10.0f, 5.0f));
        clothShader.SetVec3("u_ViewPos", camera.GetPosition());

        // Draw cloth
        clothMesh.Draw(*reinterpret_cast<Renderer*>(nullptr), clothShader);

        clothShader.Unbind();
    });

    // Run application
    app.Run();

    return 0;
}
