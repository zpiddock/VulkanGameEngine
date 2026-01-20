#pragma once

#include <memory>
#include <vector>
#include <functional>

#include "frame_info.hpp"
#include "imgui_context.hpp"
#include "render_graph.hpp"
#include "scene.hpp"
#include "config.hpp"
#include "batleth/device.hpp"
#include "batleth/instance.hpp"
#include "batleth/surface.hpp"
#include "batleth/pipeline.hpp"
#include "batleth/shader.hpp"
#include "batleth/swapchain.hpp"
#include "batleth/buffer.hpp"
#include "batleth/descriptors.hpp"
#include "render_systems/point_light_system.hpp"
#include "render_systems/simple_render_system.hpp"
#include "render_systems/blit_render_system.hpp"
#include "render_systems/depth_prepass_system.hpp"

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
    struct GlobalUbo;

    /**
     * Manages the Vulkan rendering pipeline and resources.
     * Encapsulates all Vulkan-specific rendering logic.
     */
    class KLINGON_API Renderer {
    public:
        Renderer(const KlingonConfig &config, borg::Window &window);

        ~Renderer();

        Renderer(const Renderer &) = delete;

        Renderer &operator=(const Renderer &) = delete;

        Renderer(Renderer &&) = default;

        Renderer &operator=(Renderer &&) = delete;

        auto begin_frame() -> bool;

        auto end_frame() -> void;

        auto begin_rendering() -> void;

        auto end_rendering() -> void;

        auto wait_idle() -> void;

        auto on_resize() -> void;

        auto get_current_command_buffer() const -> VkCommandBuffer { return m_command_buffers[m_current_frame]; }
        auto get_current_frame_index() const -> std::uint32_t { return m_current_frame; }

        // ImGui access (only available if enabled)
        auto get_imgui_context() -> ImGuiContext * { return m_imgui_context.get(); }
        auto has_imgui() const -> bool { return m_imgui_context != nullptr; }

        auto render_imgui(VkCommandBuffer cmd) -> void;

        // Vulkan handle getters
        auto get_instance() const -> VkInstance { return m_instance->get_handle(); }
        auto get_device() const -> VkDevice { return m_device->get_logical_device(); }
        auto get_physical_device() const -> VkPhysicalDevice { return m_device->get_physical_device(); }
        auto get_graphics_queue() const -> VkQueue { return m_device->get_graphics_queue(); }

        auto get_graphics_queue_family() const -> std::uint32_t {
            return m_device->get_queue_family_indices().graphics_family.value();
        }

        // Swapchain info
        auto get_swapchain_format() const -> VkFormat { return m_swapchain->get_format(); }
        auto get_swapchain_image_count() const -> std::size_t { return m_swapchain->get_image_count(); }
        auto get_swapchain_extent() const -> VkExtent2D { return m_swapchain->get_extent(); }
        auto get_current_swapchain_image() const -> VkImage { return m_swapchain->get_images()[m_current_image_index]; }

        auto get_current_swapchain_image_view() const -> VkImageView {
            return m_swapchain->get_image_views()[m_current_image_index];
        }

        auto get_depth_format() const -> VkFormat { return m_depth_format; }

        // Frame info
        static constexpr auto get_max_frames_in_flight() -> std::uint32_t { return MAX_FRAMES_IN_FLIGHT; }

        // Device access for render systems
        auto get_device_ref() -> batleth::Device & { return *m_device; }

        // Scene rendering (new Scene API)
        auto render_scene(Scene *scene, float delta_time) -> void;

        // ImGui callback
        using ImGuiCallback = std::function<void()>;

        auto set_imgui_callback(ImGuiCallback callback) -> void;

        // Custom render system registration
        template<typename T, typename... Args>
        auto register_render_system(Args &&... args) -> T * {
            auto system = std::make_unique<T>(std::forward<Args>(args)...);
            T *ptr = system.get();
            m_custom_render_systems.push_back(std::move(system));
            invalidate_render_graph();
            return ptr;
        }

        // Manual invalidation
        auto invalidate_render_graph() -> void;

        // Debug settings
        auto set_debug_rendering_enabled(bool enabled) -> void;

        auto is_debug_rendering_enabled() const -> bool;

    private:
        auto create_instance() -> void;

        auto create_device() -> void;

        auto create_swapchain() -> void;

        auto create_depth_resources() -> void;

        auto create_command_pool() -> void;

        auto create_command_buffers() -> void;

        auto create_sync_objects() -> void;

        auto recreate_swapchain() -> void;

        auto cleanup_depth_resources() -> void;

        auto find_depth_format() -> VkFormat;

        // Render graph and scene management
        auto build_default_render_graph() -> void;

        auto should_rebuild_render_graph() const -> bool;

        auto update_global_ubo(Scene *scene, float delta_time) -> void;

        auto update_camera_from_scene(Scene *scene, float delta_time) -> void;

        auto create_global_descriptors() -> void;


        borg::Window &m_window;
        KlingonConfig m_config;

        // Vulkan objects - ordered for proper RAII destruction
        // Destruction happens in REVERSE order of declaration
        // So declare in order: instance -> surface -> device -> resources that depend on device
        std::unique_ptr<batleth::Instance> m_instance;
        std::unique_ptr<batleth::Surface> m_surface;
        std::unique_ptr<batleth::Device> m_device;

        // Synchronization objects (destroyed before device)
        static constexpr std::uint32_t MAX_FRAMES_IN_FLIGHT = 2;
        std::vector<VkSemaphore> m_image_available_semaphores;
        std::vector<VkSemaphore> m_render_finished_semaphores;
        std::vector<VkFence> m_in_flight_fences;

        // Command buffers (destroyed before device)
        VkCommandPool m_command_pool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> m_command_buffers;

        // Rendering resources (destroyed before device)
        std::unique_ptr<batleth::Swapchain> m_swapchain;

        // Depth resources
        VkImage m_depth_image = VK_NULL_HANDLE;
        VkDeviceMemory m_depth_image_memory = VK_NULL_HANDLE;
        VkImageView m_depth_image_view = VK_NULL_HANDLE;
        VkFormat m_depth_format = VK_FORMAT_D32_SFLOAT;

        // ImGui must be destroyed before device (contains Vulkan resources)
        std::unique_ptr<ImGuiContext> m_imgui_context;

        // Frame state
        std::uint32_t m_current_frame = 0;
        std::uint32_t m_current_image_index = 0;
        bool m_framebuffer_resized = false;

        // Render graph and scene (Stage 2 additions)
        std::unique_ptr<RenderGraph> m_render_graph;
        Scene *m_active_scene = nullptr;
        VkExtent2D m_last_render_extent = {0, 0};

        // UBO and descriptors
        std::unique_ptr<batleth::DescriptorSetLayout> m_global_set_layout;
        std::unique_ptr<batleth::DescriptorPool> m_global_descriptor_pool;
        std::vector<VkDescriptorSet> m_global_descriptor_sets;
        std::vector<std::unique_ptr<batleth::Buffer> > m_ubo_buffers;
        GlobalUbo m_current_ubo;

        // Render systems
        std::unique_ptr<SimpleRenderSystem> m_simple_render_system;
        std::unique_ptr<PointLightSystem> m_point_light_system;
        std::unique_ptr<BlitRenderSystem> m_blit_render_system;
        std::unique_ptr<DepthPrepassSystem> m_depth_prepass_system;
        std::vector<std::unique_ptr<IRenderSystem> > m_custom_render_systems;
        bool m_debug_rendering_enabled = true;

        // Offscreen rendering resources
        VkSampler m_offscreen_sampler = VK_NULL_HANDLE;

        // ImGui callback
        ImGuiCallback m_imgui_callback;
    };
} // namespace klingon
