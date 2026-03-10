#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace cloth {

enum class ProjectionType {
    Perspective,
    Orthographic
};

class Camera {
public:
    Camera(ProjectionType type = ProjectionType::Perspective);

    void SetPosition(const glm::vec3& position) { m_Position = position; }
    void SetRotation(const glm::vec3& rotation) { m_Rotation = rotation; }
    void SetProjection(float aspectRatio);

    glm::vec3 GetPosition() const { return m_Position; }
    glm::vec3 GetRotation() const { return m_Rotation; }
    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const { return m_ProjectionMatrix; }
    glm::mat4 GetViewProjectionMatrix() const { return GetProjectionMatrix() * GetViewMatrix(); }

    // Camera controls
    void MoveForward(float distance);
    void MoveRight(float distance);
    void MoveUp(float distance);
    void Rotate(float yaw, float pitch);

    // Get front/right/up vectors
    glm::vec3 GetFront() const;
    glm::vec3 GetRight() const;
    glm::vec3 GetUp() const;

private:
    glm::vec3 m_Position;
    glm::vec3 m_Rotation; // Euler angles (yaw, pitch, roll)
    glm::mat4 m_ProjectionMatrix;
    ProjectionType m_ProjectionType;

    float m_FOV;
    float m_NearPlane;
    float m_FarPlane;
    float m_OrthoSize;
};

} // namespace cloth
