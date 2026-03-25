#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace cloth {

Camera::Camera(ProjectionType type)
    : m_Position(0.0f, 0.0f, 5.0f),
      m_Rotation(0.0f, 0.0f, 0.0f),
      m_ProjectionType(type),
      m_FOV(45.0f),
      m_NearPlane(0.05f),      // Tightened for better close-up precision
      m_FarPlane(500.0f),      // Reduced to 500 to improve depth buffer distribution
      m_OrthoSize(10.0f) {
}

void Camera::SetProjection(float aspectRatio) {
    if (m_ProjectionType == ProjectionType::Perspective) {
        m_ProjectionMatrix = glm::perspective(
            glm::radians(m_FOV),
            aspectRatio,
            m_NearPlane,
            m_FarPlane
        );
    } else {
        float halfSize = m_OrthoSize * 0.5f;
        m_ProjectionMatrix = glm::ortho(
            -halfSize * aspectRatio,
            halfSize * aspectRatio,
            -halfSize,
            halfSize,
            m_NearPlane,
            m_FarPlane
        );
    }
}

glm::mat4 Camera::GetViewMatrix() const {
    return glm::lookAt(
        m_Position,
        m_Position + GetFront(),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
}

void Camera::MoveForward(float distance) {
    m_Position += GetFront() * distance;
}

void Camera::MoveRight(float distance) {
    m_Position += GetRight() * distance;
}

void Camera::MoveUp(float distance) {
    m_Position += glm::vec3(0.0f, 1.0f, 0.0f) * distance;
}

void Camera::Rotate(float yaw, float pitch) {
    m_Rotation.x += yaw;
    m_Rotation.y += pitch;

    // Clamp pitch to avoid flipping
    if (m_Rotation.y > 89.0f) m_Rotation.y = 89.0f;
    if (m_Rotation.y < -89.0f) m_Rotation.y = -89.0f;
}

glm::vec3 Camera::GetFront() const {
    glm::vec3 front;
    front.x = cos(glm::radians(m_Rotation.x)) * cos(glm::radians(m_Rotation.y));
    front.y = sin(glm::radians(m_Rotation.y));
    front.z = sin(glm::radians(m_Rotation.x)) * cos(glm::radians(m_Rotation.y));
    return glm::normalize(front);
}

glm::vec3 Camera::GetRight() const {
    return glm::normalize(glm::cross(GetFront(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::vec3 Camera::GetUp() const {
    return glm::normalize(glm::cross(GetRight(), GetFront()));
}

} // namespace cloth
