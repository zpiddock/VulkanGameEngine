#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <vulkan/vulkan.h>
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

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
 * Unified engine configuration with hierarchical structure.
 * All subsystems have their own nested Config structs.
 */
struct KLINGON_API KlingonConfig {

    // ========== Application Info ==========
    struct Application {
        std::string name = "Klingon Application";
        uint32_t version_major = 1;
        uint32_t version_minor = 0;
        uint32_t version_patch = 0;

        template<class Archive>
        void serialize(Archive& ar) {
            ar(CEREAL_NVP(name),
               CEREAL_NVP(version_major),
               CEREAL_NVP(version_minor),
               CEREAL_NVP(version_patch));
        }
    } application;

    // ========== Window Configuration ==========
    struct Window {
        std::string title = "Klingon Engine";
        uint32_t width = 1920;
        uint32_t height = 1080;
        bool resizable = true;
        bool maximized = false;
        bool fullscreen = false;

        template<class Archive>
        void serialize(Archive& ar) {
            ar(CEREAL_NVP(title),
               CEREAL_NVP(width),
               CEREAL_NVP(height),
               CEREAL_NVP(resizable),
               CEREAL_NVP(maximized),
               CEREAL_NVP(fullscreen));
        }
    } window;

    // ========== Vulkan Configuration ==========
    struct Vulkan {

        // Instance settings
        struct Instance {
            bool enable_validation = true;  // Debug builds only
            std::vector<std::string> instance_extensions = {};  // Auto-detected from GLFW
            std::vector<std::string> validation_layers = {"VK_LAYER_KHRONOS_validation"};

            template<class Archive>
            void serialize(Archive& ar) {
                ar(CEREAL_NVP(enable_validation),
                   CEREAL_NVP(instance_extensions),
                   CEREAL_NVP(validation_layers));
            }
        } instance;

        // Device settings
        struct Device {
            int preferred_gpu_index = -1;  // -1 = auto-select best GPU
            std::vector<std::string> device_extensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
                VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
            };

            template<class Archive>
            void serialize(Archive& ar) {
                ar(CEREAL_NVP(preferred_gpu_index),
                   CEREAL_NVP(device_extensions));
            }
        } device;

        // Swapchain settings
        struct Swapchain {
            bool vsync = true;  // VK_PRESENT_MODE_FIFO_KHR
            uint32_t min_image_count = 2;  // Double buffering
            std::string preferred_present_mode = "mailbox";  // "immediate", "fifo", "mailbox", "fifo_relaxed"

            template<class Archive>
            void serialize(Archive& ar) {
                ar(CEREAL_NVP(vsync),
                   CEREAL_NVP(min_image_count),
                   CEREAL_NVP(preferred_present_mode));
            }
        } swapchain;

        // Shader settings
        struct Shaders {
            std::string cache_directory = "shader_cache";
            bool enable_hot_reload = true;
            bool enable_validation = true;
            bool enable_optimization = false;  // Slow compile, faster runtime

            template<class Archive>
            void serialize(Archive& ar) {
                ar(CEREAL_NVP(cache_directory),
                   CEREAL_NVP(enable_hot_reload),
                   CEREAL_NVP(enable_validation),
                   CEREAL_NVP(enable_optimization));
            }
        } shaders;

        template<class Archive>
        void serialize(Archive& ar) {
            ar(CEREAL_NVP(instance),
               CEREAL_NVP(device),
               CEREAL_NVP(swapchain),
               CEREAL_NVP(shaders));
        }
    } vulkan;

    // ========== Renderer Configuration ==========
    struct Renderer {

        // Forward+ renderer settings
        struct ForwardPlus {
            bool enabled = true;
            uint32_t tile_size = 16;  // 16x16 pixel tiles
            uint32_t max_lights_per_tile = 256;

            template<class Archive>
            void serialize(Archive& ar) {
                ar(CEREAL_NVP(enabled),
                   CEREAL_NVP(tile_size),
                   CEREAL_NVP(max_lights_per_tile));
            }
        } forward_plus;

        // Debug rendering settings
        struct Debug {
            bool enable_validation_layers = true;
            bool enable_imgui = true;
            bool enable_point_light_visualization = false;
            bool enable_wireframe = false;

            template<class Archive>
            void serialize(Archive& ar) {
                ar(CEREAL_NVP(enable_validation_layers),
                   CEREAL_NVP(enable_imgui),
                   CEREAL_NVP(enable_point_light_visualization),
                   CEREAL_NVP(enable_wireframe));
            }
        } debug;

        // Performance settings
        struct Performance {
            uint32_t max_frames_in_flight = 2;

            template<class Archive>
            void serialize(Archive& ar) {
                ar(CEREAL_NVP(max_frames_in_flight));
            }
        } performance;

        template<class Archive>
        void serialize(Archive& ar) {
            ar(CEREAL_NVP(forward_plus),
               CEREAL_NVP(debug),
               CEREAL_NVP(performance));
        }
    } renderer;

    // ========== Root Serialization ==========
    template<class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(application),
           CEREAL_NVP(window),
           CEREAL_NVP(vulkan),
           CEREAL_NVP(renderer));
    }
};

} // namespace klingon
