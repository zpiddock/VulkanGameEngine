#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#ifdef _WIN32
    #ifdef KLINGON_EXPORTS
        #define KLINGON_API __declspec(dllexport)
    #else
        #define KLINGON_API __declspec(dllimport)
    #endif
#else
    #define KLINGON_API
#endif

namespace klingon {

/**
 * Transform component for representing 3D position, rotation, and scale.
 * Uses Tait-Bryan angles for rotation (Y-X-Z order).
 */
struct KLINGON_API Transform {
    glm::vec3 translation{0.f, 0.f, 0.f};
    glm::vec3 scale{1.f, 1.f, 1.f};
    glm::vec3 rotation{0.f, 0.f, 0.f}; // Pitch, Yaw, Roll in radians

    /**
     * Compute the model matrix: Translate * Ry * Rx * Rz * Scale
     * Rotations correspond to Tait-Bryan angles of Y(1), X(2), Z(3)
     * https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
     */
    auto mat4() const -> glm::mat4;

    /**
     * Compute the normal matrix for transforming normals
     * This is the transpose of the inverse of the upper-left 3x3 of the model matrix
     */
    auto normal_matrix() const -> glm::mat3;
};

} // namespace klingon
