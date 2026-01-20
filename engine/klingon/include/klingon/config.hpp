#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <vulkan/vulkan.h>
#include <ser20/ser20.hpp>
#include <ser20/archives/json.hpp>
#include <ser20/types/string.hpp>
#include <ser20/types/vector.hpp>

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
            ar(SER20_NVP(name),
               SER20_NVP(version_major),
               SER20_NVP(version_minor),
               SER20_NVP(version_patch));
        }
    } application;

    // ========== Window Configuration ==========
    struct Window {
        uint32_t width = 1920;
        uint32_t height = 1080;
        bool resizable = true;
        bool maximized = true;
        bool fullscreen = false;

        template<class Archive>
        void serialize(Archive& ar) {
            ar(SER20_NVP(width),
               SER20_NVP(height),
               SER20_NVP(resizable),
               SER20_NVP(maximized),
               SER20_NVP(fullscreen));
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
                ar(SER20_NVP(enable_validation),
                   SER20_NVP(instance_extensions),
                   SER20_NVP(validation_layers));
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
                ar(SER20_NVP(preferred_gpu_index),
                   SER20_NVP(device_extensions));
            }
        } device;

        // Swapchain settings
        struct Swapchain {
            bool vsync = true;  // VK_PRESENT_MODE_FIFO_KHR
            uint32_t min_image_count = 2;  // Double buffering
            std::string preferred_present_mode = "mailbox";  // "immediate", "fifo", "mailbox", "fifo_relaxed"

            template<class Archive>
            void serialize(Archive& ar) {
                ar(SER20_NVP(vsync),
                   SER20_NVP(min_image_count),
                   SER20_NVP(preferred_present_mode));
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
                ar(SER20_NVP(cache_directory),
                   SER20_NVP(enable_hot_reload),
                   SER20_NVP(enable_validation),
                   SER20_NVP(enable_optimization));
            }
        } shaders;

        template<class Archive>
        void serialize(Archive& ar) {
            ar(SER20_NVP(instance),
               SER20_NVP(device),
               SER20_NVP(swapchain),
               SER20_NVP(shaders));
        }
    } vulkan;

    // ========== Renderer Configuration ==========
    struct Renderer {

        // Forward+ renderer settings
        struct ForwardPlus {
            bool enabled = true;
            uint32_t tile_size = 16;  // 16x16 pixel tiles
            uint32_t max_lights_per_tile = 256;
            bool enable_depth_prepass = true;

            template<class Archive>
            void serialize(Archive& ar) {
                ar(SER20_NVP(enabled),
                   SER20_NVP(tile_size),
                   SER20_NVP(max_lights_per_tile),
                   SER20_NVP(enable_depth_prepass));
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
                ar(SER20_NVP(enable_validation_layers),
                   SER20_NVP(enable_imgui),
                   SER20_NVP(enable_point_light_visualization),
                   SER20_NVP(enable_wireframe));
            }
        } debug;

        // Performance settings
        struct Performance {
            uint32_t max_frames_in_flight = 2;

            template<class Archive>
            void serialize(Archive& ar) {
                ar(SER20_NVP(max_frames_in_flight));
            }
        } performance;

        // Offscreen rendering settings
        struct Offscreen {
            bool enabled = true;
            std::string color_format = "rgba16f";  // "rgba8", "rgba16f", "rgba32f"

            template<class Archive>
            void serialize(Archive& ar) {
                ar(SER20_NVP(enabled),
                   SER20_NVP(color_format));
            }
        } offscreen;

        template<class Archive>
        void serialize(Archive& ar) {
            ar(SER20_NVP(forward_plus),
               SER20_NVP(debug),
               SER20_NVP(performance),
               SER20_NVP(offscreen));
        }
    } renderer;

    // ========== Root Serialization ==========
    template<class Archive>
    void serialize(Archive& ar) {
        ar(SER20_NVP(application),
           SER20_NVP(window),
           SER20_NVP(vulkan),
           SER20_NVP(renderer));
    }
};

} // namespace klingon
