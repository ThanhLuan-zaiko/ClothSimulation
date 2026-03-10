#pragma once

#include <glm/glm.hpp>
#include <random>

namespace cloth {

class MathUtils {
public:
    // Clamp value between min and max
    template<typename T>
    static T Clamp(T value, T min, T max) {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }

    // Linear interpolation
    static float Lerp(float a, float b, float t) {
        return a + (b - a) * Clamp(t, 0.0f, 1.0f);
    }

    // Vector lerp
    static glm::vec3 Lerp(const glm::vec3& a, const glm::vec3& b, float t) {
        return glm::mix(a, b, Clamp(t, 0.0f, 1.0f));
    }

    // Random float between min and max
    static float RandomFloat(float min = 0.0f, float max = 1.0f) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(min, max);
        return dis(gen);
    }

    // Random vector in a range
    static glm::vec3 RandomVector(const glm::vec3& min, const glm::vec3& max) {
        return glm::vec3(
            RandomFloat(min.x, max.x),
            RandomFloat(min.y, max.y),
            RandomFloat(min.z, max.z)
        );
    }

    // Check if two floats are approximately equal
    static bool ApproximatelyEqual(float a, float b, float epsilon = 0.0001f) {
        return glm::abs(a - b) < epsilon;
    }

    // Convert degrees to radians
    static float ToRadians(float degrees) {
        return glm::radians(degrees);
    }

    // Convert radians to degrees
    static float ToDegrees(float radians) {
        return glm::degrees(radians);
    }

    // Calculate distance between two points
    static float Distance(const glm::vec3& a, const glm::vec3& b) {
        return glm::length(b - a);
    }

    // Calculate squared distance (faster, no sqrt)
    static float DistanceSquared(const glm::vec3& a, const glm::vec3& b) {
        glm::vec3 diff = b - a;
        return glm::dot(diff, diff);
    }
};

} // namespace cloth
