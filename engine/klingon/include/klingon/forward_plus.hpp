#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <cstdint>

namespace klingon {

// Light grid dimensions (computed from screen resolution and tile size)
struct LightGridDimensions {
    uint32_t tile_count_x;
    uint32_t tile_count_y;
    uint32_t max_lights_per_tile;
    uint32_t tile_size;  // pixels (16x16 typical)
};

// Light culling push constants (passed to compute shader)
struct LightCullingPushConstants {
    glm::mat4 view_projection_inverse;  // Reconstruct world pos from depth
    glm::uvec2 screen_size;             // pixels
    glm::uvec2 tile_count;              // grid dimensions
    uint32_t num_lights;
    uint32_t tile_size;
    float z_near;
    float z_far;
};

// Forward+ push constants for shading pass
struct ForwardPlusPushConstants {
    glm::mat4 model_matrix;
    glm::mat4 normal_matrix;
    glm::uvec2 tile_count;
    uint32_t tile_size;
    uint32_t max_lights_per_tile;
};

} // namespace klingon
