#pragma once

#include <vulkan/vulkan_core.h>
#include <unordered_map>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "camera.hpp"
#include "game_object.hpp"

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

// Forward declarations

constexpr int MAX_LIGHTS = 10;

/**
 * Point light data structure for UBO
 */
struct PointLight {
    glm::vec4 position{};  // w is radius
    glm::vec4 color{};     // w is intensity
};

/**
 * Global uniform buffer object containing scene-wide data
 * Must match std140 layout in shaders
 */
struct alignas(16) GlobalUbo {
    glm::mat4 projection{1.f};      // offset 0,   size 64
    glm::mat4 view{1.f};            // offset 64,  size 64
    glm::vec4 ambient_light_color{1.f, 1.f, 1.f, 0.02f};  // offset 128, size 16
    PointLight point_lights[MAX_LIGHTS];  // offset 144, size 320 (10 * 32)
    int num_lights{0};              // offset 464, size 4
    // Padding to align struct to 16 bytes (std140 requirement)
    int _padding[3]{};              // offset 468, size 12
};  // Total size: 480 bytes

/**
 * Frame information passed to render systems
 * Contains all data needed to render a frame
 */
struct KLINGON_API FrameInfo {
    int frame_index;
    float frame_time;
    VkCommandBuffer command_buffer;
    Camera& camera;
    VkDescriptorSet global_descriptor_set;
    std::unordered_map<unsigned int, GameObject>& game_objects;
};

} // namespace klingon
