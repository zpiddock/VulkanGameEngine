#include "klingon/renderer.hpp"
#include "klingon/imgui_context.hpp"
#include "klingon/scene.hpp"
#include "klingon/render_graph.hpp"
#include "klingon/frame_info.hpp"
#include "klingon/forward_plus.hpp"
#include "klingon/texture_manager.hpp"
#include "klingon/render_systems/simple_render_system.hpp"
#include "klingon/render_systems/point_light_system.hpp"
#include "borg/window.hpp"
#include "batleth/instance.hpp"
#include "batleth/device.hpp"
#include "batleth/swapchain.hpp"
#include "batleth/shader.hpp"
#include "batleth/pipeline.hpp"
#include "batleth/buffer.hpp"
#include "batleth/descriptors.hpp"
#include "federation/log.hpp"

#include <GLFW/glfw3.h>
#include <imgui_impl_vulkan.h>
#include <glm/gtc/constants.hpp>
#include <array>
#include <stdexcept>
#include <vector>

#include <vk_mem_alloc.h>
#include "vma/vk_mem_alloc.h"

namespace klingon {
    Renderer::Renderer(const KlingonConfig &config, borg::Window &window)
        : m_window(window), m_config(config) {
        FED_INFO("Initializing renderer");

        create_instance();
        create_device();

        // Create TextureManager early (needed for asset loading)
        FED_INFO("Creating TextureManager");
        TextureManager::Config tex_config{
        .device = *m_device,
        .allocator = get_allocator(),  // Use getter to ensure allocator is initialized
        .max_textures = 4096,
        .max_materials = 1024,
        };
        m_texture_manager = std::make_unique<TextureManager>(tex_config);

        create_swapchain();
        create_depth_resources();
        create_command_pool();
        create_command_buffers();
        create_sync_objects();

        // Create offscreen sampler (if offscreen rendering enabled)
        if (m_config.renderer.offscreen.enabled) {
            VkSamplerCreateInfo sampler_info{};
            sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            sampler_info.magFilter = VK_FILTER_LINEAR;
            sampler_info.minFilter = VK_FILTER_LINEAR;
            sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.anisotropyEnable = VK_FALSE;
            sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            sampler_info.unnormalizedCoordinates = VK_FALSE;
            sampler_info.compareEnable = VK_FALSE;
            sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
            sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

            if (::vkCreateSampler(m_device->get_logical_device(), &sampler_info, nullptr,
                                  &m_offscreen_sampler) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create offscreen sampler");
            }
        }

        // Initialize ImGui if requested
        if (m_config.renderer.debug.enable_imgui) {
            FED_INFO("Initializing ImGui");
            m_imgui_context = std::make_unique<ImGuiContext>(
                m_window.get_native_handle(),
                *m_device,
                m_swapchain->get_format(),
                m_depth_format,
                m_swapchain->get_image_count()
            );
        }

        // Note: Forward+ compute pipeline is created lazily when render graph is built
        // (after global descriptors are set up)

        FED_INFO("Renderer initialized successfully");
    }

    Renderer::~Renderer() {
        FED_DEBUG("Destroying renderer");

        // Wait for GPU to finish before cleanup
        if (m_device) {
            m_device->wait_idle();
        }

        // Cleanup raw Vulkan handles that don't have RAII wrappers
        // These are the only resources requiring manual cleanup
        for (auto semaphore: m_image_available_semaphores) {
            ::vkDestroySemaphore(m_device->get_logical_device(), semaphore, nullptr);
        }
        for (auto semaphore: m_render_finished_semaphores) {
            ::vkDestroySemaphore(m_device->get_logical_device(), semaphore, nullptr);
        }
        for (auto fence: m_in_flight_fences) {
            ::vkDestroyFence(m_device->get_logical_device(), fence, nullptr);
        }

        if (m_command_pool != VK_NULL_HANDLE) {
            ::vkDestroyCommandPool(m_device->get_logical_device(), m_command_pool, nullptr);
        }

        // Cleanup offscreen sampler
        if (m_offscreen_sampler != VK_NULL_HANDLE) {
            ::vkDestroySampler(m_device->get_logical_device(), m_offscreen_sampler, nullptr);
        }

        // Cleanup Forward+ resources
        cleanup_forward_plus_resources();

        // Cleanup depth resources
        cleanup_depth_resources();

        // All RAII-wrapped resources (swapchain, surface, imgui_context, pipeline,
        // shaders, device, instance) are automatically destroyed in correct order
        // via C++ member destruction (reverse of declaration order)

        FED_DEBUG("Renderer destroyed successfully");
    }

    auto Renderer::begin_frame() -> bool {
        // Wait for the previous frame to finish
        ::vkWaitForFences(m_device->get_logical_device(), 1, &m_in_flight_fences[m_current_frame], VK_TRUE, UINT64_MAX);

        // Acquire next image from swapchain
        VkResult result = ::vkAcquireNextImageKHR(
            m_device->get_logical_device(),
            m_swapchain->get_handle(),
            UINT64_MAX,
            m_image_available_semaphores[m_current_frame],
            VK_NULL_HANDLE,
            &m_current_image_index
        );

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            // Swapchain is out of date (window resized), recreate and skip this frame
            recreate_swapchain();
            return false;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("Failed to acquire swapchain image");
        }

        // Reset fence only if we're submitting work
        ::vkResetFences(m_device->get_logical_device(), 1, &m_in_flight_fences[m_current_frame]);

        // Reset command buffer
        ::vkResetCommandBuffer(m_command_buffers[m_current_frame], 0);

        // Begin ImGui frame if enabled
        // User code can now call ImGui functions
        if (m_imgui_context) {
            m_imgui_context->begin_frame();
        }

        return true;
    }

    auto Renderer::begin_rendering() -> void {
        // Begin recording command buffer
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = 0;
        begin_info.pInheritanceInfo = nullptr;

        if (::vkBeginCommandBuffer(m_command_buffers[m_current_frame], &begin_info) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer");
        }

        // Transition image to color attachment optimal
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_swapchain->get_images()[m_current_image_index];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // Transition depth image to depth attachment optimal
        VkImageMemoryBarrier depth_barrier{};
        depth_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        depth_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depth_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depth_barrier.image = m_depth_image;
        depth_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depth_barrier.subresourceRange.baseMipLevel = 0;
        depth_barrier.subresourceRange.levelCount = 1;
        depth_barrier.subresourceRange.baseArrayLayer = 0;
        depth_barrier.subresourceRange.layerCount = 1;
        depth_barrier.srcAccessMask = 0;
        depth_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array barriers = {barrier, depth_barrier};

        ::vkCmdPipelineBarrier(
            m_command_buffers[m_current_frame],
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            0,
            0, nullptr,
            0, nullptr,
            barriers.size(), barriers.data()
        );

        // Begin dynamic rendering
        VkRenderingAttachmentInfo color_attachment{};
        color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_attachment.imageView = m_swapchain->get_image_views()[m_current_image_index];
        color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.clearValue.color = {{0.01f, 0.01f, 0.01f, 1.0f}}; // Dark background

        // Depth attachment
        VkRenderingAttachmentInfo depth_attachment{};
        depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_attachment.imageView = m_depth_image_view;
        depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.clearValue.depthStencil = {1.0f, 0};

        VkRenderingInfo rendering_info{};
        rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering_info.renderArea.offset = {0, 0};
        rendering_info.renderArea.extent = m_swapchain->get_extent();
        rendering_info.layerCount = 1;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachments = &color_attachment;
        rendering_info.pDepthAttachment = &depth_attachment;

        ::vkCmdBeginRendering(m_command_buffers[m_current_frame], &rendering_info);

        // Set viewport
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_swapchain->get_extent().width);
        viewport.height = static_cast<float>(m_swapchain->get_extent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        ::vkCmdSetViewport(m_command_buffers[m_current_frame], 0, 1, &viewport);

        // Set scissor
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_swapchain->get_extent();
        ::vkCmdSetScissor(m_command_buffers[m_current_frame], 0, 1, &scissor);
    }

    auto Renderer::end_rendering() -> void {
        // Render ImGui if enabled
        if (m_imgui_context) {
            m_imgui_context->render(m_command_buffers[m_current_frame]);
        }

        // End dynamic rendering
        ::vkCmdEndRendering(m_command_buffers[m_current_frame]);

        // Transition image to present
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_swapchain->get_images()[m_current_image_index];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;

        ::vkCmdPipelineBarrier(
            m_command_buffers[m_current_frame],
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        // End command buffer recording
        if (::vkEndCommandBuffer(m_command_buffers[m_current_frame]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer");
        }
    }

    auto Renderer::end_frame() -> void {
        // End ImGui frame (prepares draw data)
        if (m_imgui_context) {
            m_imgui_context->end_frame();
        }

        // Submit command buffer
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore wait_semaphores[] = {m_image_available_semaphores[m_current_frame]};
        VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &m_command_buffers[m_current_frame];

        VkSemaphore signal_semaphores[] = {m_render_finished_semaphores[m_current_frame]};
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;

        if (::vkQueueSubmit(m_device->get_graphics_queue(), 1, &submit_info, m_in_flight_fences[m_current_frame]) !=
            VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer");
        }

        // Present the image
        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = signal_semaphores;

        VkSwapchainKHR swapchains[] = {m_swapchain->get_handle()};
        present_info.swapchainCount = 1;
        present_info.pSwapchains = swapchains;
        present_info.pImageIndices = &m_current_image_index;
        present_info.pResults = nullptr;

        VkResult result = ::vkQueuePresentKHR(m_device->get_present_queue(), &present_info);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebuffer_resized) {
            // Swapchain needs to be recreated
            m_framebuffer_resized = false;
            recreate_swapchain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swapchain image");
        }

        // Advance to next frame
        m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    auto Renderer::wait_idle() -> void {
        if (m_device) {
            m_device->wait_idle();
        }
    }

    auto Renderer::render_imgui(VkCommandBuffer cmd) -> void {
        if (m_imgui_context) {
            m_imgui_context->render(cmd);
        }
    }

    auto Renderer::on_resize() -> void {
        FED_DEBUG("Window resized, marking for swapchain recreation");
        m_framebuffer_resized = true;
    }

    auto Renderer::recreate_swapchain() -> void {
        FED_INFO("Recreating swapchain");

        // Wait for device to be idle
        m_device->wait_idle();

        // Cleanup old swapchain-dependent resources
        cleanup_depth_resources();
        m_swapchain.reset();

        // Recreate swapchain and dependent resources
        create_swapchain();
        create_depth_resources();

        FED_INFO("Swapchain recreated successfully");
    }

    auto Renderer::create_instance() -> void {
        FED_DEBUG("Creating Vulkan instance");

        // Check if Vulkan is supported by GLFW
        if (!::glfwVulkanSupported()) {
            FED_ERROR("Vulkan is not supported by GLFW");
            throw std::runtime_error("Vulkan is not supported");
        }

        FED_DEBUG("Vulkan is supported");

        // List all available Vulkan extensions
        std::uint32_t extension_count = 0;
        ::vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> available_extensions(extension_count);
        ::vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, available_extensions.data());

        FED_INFO("Available Vulkan extensions ({}):", extension_count);
        for (const auto &extension: available_extensions) {
            FED_TRACE(" - {} (version {})", extension.extensionName, extension.specVersion);
        }

        // Get required GLFW extensions
        std::uint32_t glfw_extension_count = 0;
        const char **glfw_extensions = ::glfwGetRequiredInstanceExtensions(&glfw_extension_count);

        if (!glfw_extensions) {
            FED_ERROR("Failed to get required GLFW extensions");
            throw std::runtime_error("GLFW could not find required Vulkan extensions");
        }

        std::vector extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

        FED_DEBUG("GLFW requires {} Vulkan extensions", glfw_extension_count);
        for (std::uint32_t i = 0; i < glfw_extension_count; ++i) {
            FED_DEBUG(" - {}", glfw_extensions[i]);
        }

        // Add debug utils extension if validation is enabled
        if (m_config.vulkan.instance.enable_validation) {
            extensions.push_back("VK_EXT_debug_utils");
            FED_DEBUG("  - VK_EXT_debug_utils (for validation)");
        }

        batleth::Instance::Config instance_config{};
        instance_config.application_name = m_config.application.name;
        instance_config.application_version = VK_MAKE_VERSION(
            m_config.application.version_major,
            m_config.application.version_minor,
            m_config.application.version_patch
        );
        instance_config.engine_name = "Klingon Engine";
        instance_config.engine_version = 1;
        instance_config.extensions = extensions;
        instance_config.enable_validation = m_config.vulkan.instance.enable_validation;

        if (m_config.vulkan.instance.enable_validation) {
            instance_config.validation_layers = {"VK_LAYER_KHRONOS_validation"};
        }

        m_instance = std::make_unique<batleth::Instance>(instance_config);
    }

    auto Renderer::create_device() -> void {
        FED_DEBUG("Creating Vulkan device");

        // Create window surface using RAII wrapper
        // Surface accepts Borg::Window reference, maintaining proper layering
        m_surface = std::make_unique<batleth::Surface>(m_instance->get_handle(), m_window);

        batleth::Device::Config device_config{};
        device_config.instance = m_instance->get_handle();
        device_config.surface = m_surface->get_handle();
        for (auto& extension : m_config.vulkan.device.device_extensions) {
            device_config.device_extensions.push_back(extension.c_str());
        }

        m_device = std::make_unique<batleth::Device>(device_config);
    }

    auto Renderer::create_swapchain() -> void {
        FED_DEBUG("Creating Vulkan swapchain");

        auto [width, height] = m_window.get_framebuffer_size();

        // Wait while window is minimized
        while (width == 0 || height == 0) {
            std::tie(width, height) = m_window.get_framebuffer_size();
            m_window.wait_events();
        }

        batleth::Swapchain::Config swapchain_config{};
        swapchain_config.device = m_device->get_logical_device();
        swapchain_config.physical_device = m_device->get_physical_device();
        swapchain_config.surface = m_device->get_surface();
        swapchain_config.width = width;
        swapchain_config.height = height;

        m_swapchain = std::make_unique<batleth::Swapchain>(swapchain_config);
    }

    auto Renderer::create_command_pool() -> void {
        FED_DEBUG("Creating command pool");

        auto queue_family_indices = m_device->get_queue_family_indices();

        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = queue_family_indices.graphics_family.value();

        if (::vkCreateCommandPool(m_device->get_logical_device(), &pool_info, nullptr, &m_command_pool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool");
        }

        // Set command pool on device for single-time commands (staging buffers, etc.)
        m_device->set_command_pool(m_command_pool);

        FED_DEBUG("Command pool created successfully");
    }

    auto Renderer::create_command_buffers() -> void {
        FED_DEBUG("Creating command buffers");

        m_command_buffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = m_command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = static_cast<std::uint32_t>(m_command_buffers.size());

        if (::vkAllocateCommandBuffers(m_device->get_logical_device(), &alloc_info, m_command_buffers.data()) !=
            VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers");
        }

        FED_DEBUG("Command buffers created successfully");
    }

    auto Renderer::create_sync_objects() -> void {
        FED_DEBUG("Creating synchronization objects");

        m_image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled so first frame doesn't wait

        for (std::uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (::vkCreateSemaphore(m_device->get_logical_device(), &semaphore_info, nullptr,
                                    &m_image_available_semaphores[i]) != VK_SUCCESS ||
                ::vkCreateSemaphore(m_device->get_logical_device(), &semaphore_info, nullptr,
                                    &m_render_finished_semaphores[i]) != VK_SUCCESS ||
                ::vkCreateFence(m_device->get_logical_device(), &fence_info, nullptr, &m_in_flight_fences[i]) !=
                VK_SUCCESS) {
                throw std::runtime_error("Failed to create synchronization objects");
            }
        }

        FED_DEBUG("Synchronization objects created successfully");
    }


    auto Renderer::create_depth_resources() -> void {
        FED_DEBUG("Creating depth resources");

        m_depth_format = find_depth_format();
        auto extent = m_swapchain->get_extent();

        // Create depth image
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = extent.width;
        image_info.extent.height = extent.height;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = m_depth_format;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (::vkCreateImage(m_device->get_logical_device(), &image_info, nullptr, &m_depth_image) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create depth image");
        }

        // Allocate memory for depth image
        VkMemoryRequirements mem_requirements;
        ::vkGetImageMemoryRequirements(m_device->get_logical_device(), m_depth_image, &mem_requirements);

        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_requirements.size;
        alloc_info.memoryTypeIndex = m_device->find_memory_type(
            mem_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        if (::vkAllocateMemory(m_device->get_logical_device(), &alloc_info, nullptr, &m_depth_image_memory) !=
            VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate depth image memory");
        }

        ::vkBindImageMemory(m_device->get_logical_device(), m_depth_image, m_depth_image_memory, 0);

        // Create depth image view
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = m_depth_image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = m_depth_format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        if (::vkCreateImageView(m_device->get_logical_device(), &view_info, nullptr, &m_depth_image_view) !=
            VK_SUCCESS) {
            throw std::runtime_error("Failed to create depth image view");
        }

        FED_DEBUG("Depth resources created successfully");
    }

    auto Renderer::cleanup_depth_resources() -> void {
        if (m_depth_image_view != VK_NULL_HANDLE) {
            ::vkDestroyImageView(m_device->get_logical_device(), m_depth_image_view, nullptr);
            m_depth_image_view = VK_NULL_HANDLE;
        }
        if (m_depth_image != VK_NULL_HANDLE) {
            ::vkDestroyImage(m_device->get_logical_device(), m_depth_image, nullptr);
            m_depth_image = VK_NULL_HANDLE;
        }
        if (m_depth_image_memory != VK_NULL_HANDLE) {
            ::vkFreeMemory(m_device->get_logical_device(), m_depth_image_memory, nullptr);
            m_depth_image_memory = VK_NULL_HANDLE;
        }
    }

    auto Renderer::find_depth_format() -> VkFormat {
        std::vector candidates = {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT
        };

        for (VkFormat format: candidates) {
            VkFormatProperties props;
            ::vkGetPhysicalDeviceFormatProperties(m_device->get_physical_device(), format, &props);

            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                return format;
            }
        }

        throw std::runtime_error("Failed to find supported depth format");
    }

    // ===== Scene Rendering API (Stage 2) =====

    auto Renderer::create_global_descriptors() -> void {
        FED_INFO("Creating global descriptors for scene rendering");

        // Create descriptor set layout
        // NOTE: Includes compute shader stage for Forward+ light culling
        m_global_set_layout = batleth::DescriptorSetLayout::Builder(m_device->get_logical_device())
                .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                             VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT)
                .build();

        // Create UBO buffers (one per frame in flight)
        m_ubo_buffers.clear();
        for (std::uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            auto buffer = std::make_unique<batleth::Buffer>(
                *m_device,
                sizeof(GlobalUbo),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            );
            buffer->map();
            m_ubo_buffers.push_back(std::move(buffer));
        }

        // Create descriptor pool
        m_global_descriptor_pool = batleth::DescriptorPool::Builder(m_device->get_logical_device())
                .set_max_sets(MAX_FRAMES_IN_FLIGHT)
                .add_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT)
                .build();

        // Allocate and write descriptor sets
        m_global_descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);
        for (std::uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            auto buffer_info = m_ubo_buffers[i]->descriptor_info();
            batleth::DescriptorWriter(*m_global_set_layout, *m_global_descriptor_pool)
                    .write_buffer(0, &buffer_info)
                    .build(m_global_descriptor_sets[i]);
        }

        FED_INFO("Created {} global descriptor sets", m_global_descriptor_sets.size());
    }

    auto Renderer::create_forward_plus_compute_pipeline() -> void {
        if (!m_config.renderer.forward_plus.enabled) {
            return;
        }

        FED_INFO("Creating Forward+ compute pipeline for light culling");

        // Create depth sampler for sampling depth texture in compute shader
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_NEAREST;
        sampler_info.minFilter = VK_FILTER_NEAREST;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.anisotropyEnable = VK_FALSE;
        sampler_info.maxAnisotropy = 1.0f;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 0.0f;

        if (::vkCreateSampler(m_device->get_logical_device(), &sampler_info, nullptr, &m_depth_sampler) != VK_SUCCESS) {
            FED_ERROR("Failed to create depth sampler for Forward+ compute");
            return;
        }

        // Create descriptor set layout for Set 1 (Forward+ resources)
        // Binding 0: Depth texture (sampler2D) - compute only
        // Binding 1: Light grid (storage buffer) - compute writes, fragment reads
        // Binding 2: Light count (storage buffer) - compute writes, fragment reads
        m_forward_plus_set_layout = batleth::DescriptorSetLayout::Builder(m_device->get_logical_device())
                .add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                .add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                             VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                .add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                             VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                .build();

        // Create descriptor pool for Forward+ descriptors
        m_forward_plus_descriptor_pool = batleth::DescriptorPool::Builder(m_device->get_logical_device())
                .set_max_sets(MAX_FRAMES_IN_FLIGHT)
                .add_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT)
                .add_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT * 2) // 2 buffers per frame
                .build();

        FED_INFO("Created Forward+ descriptor set layout and pool");

        // Note: Descriptor sets will be allocated and written during render graph execution
        // because they need to reference the actual depth texture and storage buffers
        // which are created by the render graph

        FED_DEBUG("Creating pipeline layout...");

        // Create pipeline layout with push constants
        VkPushConstantRange push_constant_range{};
        push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        push_constant_range.offset = 0;
        push_constant_range.size = sizeof(LightCullingPushConstants);

        std::vector<VkDescriptorSetLayout> descriptor_set_layouts = {
            m_global_set_layout->get_layout(),       // Set 0: Global UBO
            m_forward_plus_set_layout->get_layout()  // Set 1: Forward+ resources
        };

        VkPipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size());
        pipeline_layout_info.pSetLayouts = descriptor_set_layouts.data();
        pipeline_layout_info.pushConstantRangeCount = 1;
        pipeline_layout_info.pPushConstantRanges = &push_constant_range;

        FED_DEBUG("Calling vkCreatePipelineLayout...");
        if (::vkCreatePipelineLayout(m_device->get_logical_device(), &pipeline_layout_info, nullptr,
                                     &m_light_culling_pipeline_layout) != VK_SUCCESS) {
            FED_ERROR("Failed to create light culling pipeline layout");
            return;
        }
        FED_DEBUG("Pipeline layout created successfully");

        // Load and compile compute shader
        try {
            FED_DEBUG("Configuring compute shader...");
            auto compute_shader_config = batleth::Shader::Config{};
            compute_shader_config.device = m_device->get_logical_device();
            compute_shader_config.filepath = "assets/shaders/light_culling.comp";
            compute_shader_config.stage = batleth::Shader::Stage::Compute;
            compute_shader_config.enable_hot_reload = false;  // Disable hot-reload for now
            compute_shader_config.optimize = true;

            FED_DEBUG("Compiling compute shader...");
            auto compute_shader = batleth::Shader{compute_shader_config};
            FED_DEBUG("Compute shader compiled successfully");

            // Create compute pipeline
            VkComputePipelineCreateInfo pipeline_info{};
            pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            pipeline_info.stage.stage = compute_shader.get_stage();
            pipeline_info.stage.module = compute_shader.get_module();
            pipeline_info.stage.pName = "main";
            pipeline_info.layout = m_light_culling_pipeline_layout;

            if (::vkCreateComputePipelines(m_device->get_logical_device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                                           &m_light_culling_pipeline) != VK_SUCCESS) {
                FED_ERROR("Failed to create light culling compute pipeline");
                return;
            }

            FED_INFO("Forward+ light culling compute pipeline created successfully");
        } catch (const std::exception &e) {
            FED_ERROR("Exception while creating Forward+ compute pipeline: {}", e.what());
            return;
        }
    }

    auto Renderer::cleanup_forward_plus_resources() -> void {
        if (m_light_culling_pipeline != VK_NULL_HANDLE) {
            ::vkDestroyPipeline(m_device->get_logical_device(), m_light_culling_pipeline, nullptr);
            m_light_culling_pipeline = VK_NULL_HANDLE;
        }

        if (m_light_culling_pipeline_layout != VK_NULL_HANDLE) {
            ::vkDestroyPipelineLayout(m_device->get_logical_device(), m_light_culling_pipeline_layout, nullptr);
            m_light_culling_pipeline_layout = VK_NULL_HANDLE;
        }

        if (m_depth_sampler != VK_NULL_HANDLE) {
            ::vkDestroySampler(m_device->get_logical_device(), m_depth_sampler, nullptr);
            m_depth_sampler = VK_NULL_HANDLE;
        }

        m_forward_plus_set_layout.reset();
        m_forward_plus_descriptor_pool.reset();
        m_forward_plus_descriptor_sets.clear();
    }

    auto Renderer::update_camera_from_scene(Scene *scene, float delta_time) -> void {
        if (!scene) return;

        auto &camera = scene->get_camera();
        auto &transform = scene->get_camera_transform();

        // Update view matrix from transform
        camera.set_view_yxz(transform.translation, transform.rotation);

        // Update projection from window size
        auto extent = m_swapchain->get_extent();
        float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
        camera.set_perspective_projection(
            glm::radians(60.0f), // FOV
            aspect,
            0.1f, // Near plane
            100.0f // Far plane
        );
    }

    auto Renderer::update_global_ubo(Scene *scene, float delta_time) -> void {
        if (!scene) return;

        auto &camera = scene->get_camera();

        // Update camera matrices
        m_current_ubo.projection = camera.get_projection();
        m_current_ubo.view = camera.get_view();
        m_current_ubo.inverseView = camera.get_inverse_view();
        m_current_ubo.ambient_light_color = scene->get_ambient_light();

        // Update lights via PointLightSystem
        if (m_point_light_system) {
            FrameInfo temp_info{
                static_cast<int>(m_current_frame),
                delta_time, // delta_time not needed for update
                VK_NULL_HANDLE, // command buffer not needed for update
                camera,
                VK_NULL_HANDLE, // descriptor set not needed for update
                VK_NULL_HANDLE, // texture descriptor set not needed for update
                scene->get_game_objects()
            };
            m_point_light_system->update(temp_info, m_current_ubo);
        }

        // Write to device buffer
        m_ubo_buffers[m_current_frame]->write_to_buffer(&m_current_ubo);
        m_ubo_buffers[m_current_frame]->flush();
    }

    auto Renderer::build_default_render_graph() -> void {
        FED_INFO("Building Forward+ render graph");

        auto extent = m_swapchain->get_extent();

        // Create descriptors if not already created
        if (!m_global_set_layout) {
            create_global_descriptors();
        }

        // TextureManager is already created in constructor

        // Create Forward+ compute pipeline if enabled and not already created
        if (m_config.renderer.forward_plus.enabled && m_light_culling_pipeline == VK_NULL_HANDLE) {
            create_forward_plus_compute_pipeline();
        }

        // Determine render target format (offscreen HDR or swapchain)
        VkFormat render_target_format = m_swapchain->get_format();
        if (m_config.renderer.offscreen.enabled) {
            // Use offscreen HDR format
            if (m_config.renderer.offscreen.color_format == "rgba8") {
                render_target_format = VK_FORMAT_R8G8B8A8_UNORM;
            } else if (m_config.renderer.offscreen.color_format == "rgba32f") {
                render_target_format = VK_FORMAT_R32G32B32A32_SFLOAT;
            } else {  // rgba16f (default)
                render_target_format = VK_FORMAT_R16G16B16A16_SFLOAT;
            }
        }

        // Create render systems if not already created
        if (!m_simple_render_system) {
            m_simple_render_system = std::make_unique<SimpleRenderSystem>(
                *m_device,
                render_target_format,
                m_global_set_layout->get_layout(),
                m_forward_plus_set_layout ? m_forward_plus_set_layout->get_layout() : VK_NULL_HANDLE,
                m_texture_manager->get_descriptor_layout(),
                m_config.renderer.forward_plus.enabled
            );
        }

        if (!m_point_light_system) {
            m_point_light_system = std::make_unique<PointLightSystem>(
                *m_device,
                render_target_format,
                m_global_set_layout->get_layout()
            );
        }

        if (!m_blit_render_system && m_config.renderer.offscreen.enabled) {
            m_blit_render_system = std::make_unique<BlitRenderSystem>(
                *m_device,
                m_swapchain->get_format()
            );
        }

        if (!m_depth_prepass_system && m_config.renderer.forward_plus.enable_depth_prepass) {
            m_depth_prepass_system = std::make_unique<DepthPrepassSystem>(
                *m_device,
                m_depth_format,
                m_global_set_layout->get_layout()
            );
        }

        // Create render graph
        m_render_graph = std::make_unique<RenderGraph>(*this);

        auto &builder = m_render_graph->begin_build();

        // Compute Forward+ tile dimensions
        uint32_t tile_size = m_config.renderer.forward_plus.tile_size;
        uint32_t tile_count_x = (extent.width + tile_size - 1) / tile_size;
        uint32_t tile_count_y = (extent.height + tile_size - 1) / tile_size;
        uint32_t max_lights_per_tile = m_config.renderer.forward_plus.max_lights_per_tile;

        FED_DEBUG("Forward+ configuration: tiles={}x{}, tile_size={}, max_lights_per_tile={}",
                  tile_count_x, tile_count_y, tile_size, max_lights_per_tile);

        // Create resources

        // Depth buffer (shared across passes)
        // NOTE: Must set is_transient = false because we need SAMPLED_BIT for compute shader
        auto depth_desc = batleth::ImageResourceDesc::create_2d(
            m_depth_format,
            extent.width,
            extent.height,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT  // Can be sampled in compute shader
        );
        depth_desc.is_transient = false;  // Cannot be transient with SAMPLED_BIT
        auto depth_buffer = builder.create_image("depth", depth_desc);

        // Light grid storage buffers (for Forward+ light culling)
        batleth::ResourceHandle light_grid;
        batleth::ResourceHandle light_count;

        if (m_config.renderer.forward_plus.enabled) {
            // Light grid: stores light indices for each tile
            // Size: tile_count_x * tile_count_y * max_lights_per_tile * sizeof(uint32_t)
            uint32_t light_grid_size = tile_count_x * tile_count_y * max_lights_per_tile * sizeof(uint32_t);
            light_grid = builder.create_buffer(
                "light_grid",
                batleth::BufferResourceDesc{
                    .size = light_grid_size,
                    .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    .is_transient = true  // Only needed during this frame
                }
            );

            // Light count: stores number of lights per tile
            // Size: tile_count_x * tile_count_y * sizeof(uint32_t)
            uint32_t light_count_size = tile_count_x * tile_count_y * sizeof(uint32_t);
            light_count = builder.create_buffer(
                "light_count",
                batleth::BufferResourceDesc{
                    .size = light_count_size,
                    .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    .is_transient = true
                }
            );

            FED_DEBUG("Created Forward+ light buffers: grid={} bytes, count={} bytes",
                      light_grid_size, light_count_size);
        }

        // Offscreen color buffer (if enabled)
        batleth::ResourceHandle color_target;
        if (m_config.renderer.offscreen.enabled) {
            VkFormat color_format = VK_FORMAT_R16G16B16A16_SFLOAT;  // HDR default
            if (m_config.renderer.offscreen.color_format == "rgba8") {
                color_format = VK_FORMAT_R8G8B8A8_UNORM;
            } else if (m_config.renderer.offscreen.color_format == "rgba32f") {
                color_format = VK_FORMAT_R32G32B32A32_SFLOAT;
            }

            // NOTE: Must set is_transient = false because we need SAMPLED_BIT for blit
            auto offscreen_desc = batleth::ImageResourceDesc::create_2d(
                color_format,
                extent.width,
                extent.height,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_SAMPLED_BIT  // Can be sampled for post-processing
            );
            offscreen_desc.is_transient = false;  // Cannot be transient with SAMPLED_BIT
            color_target = builder.create_image("offscreen_color", offscreen_desc);
            m_offscreen_color_handle = color_target;  // Store for viewport access

            FED_DEBUG("Created offscreen color buffer: format={}",
                      m_config.renderer.offscreen.color_format.c_str());
        } else {
            // Use backbuffer directly (no offscreen rendering)
            color_target = m_render_graph->get_backbuffer_handle();
            FED_DEBUG("Using backbuffer directly (offscreen rendering disabled)");
        }

        // Get backbuffer handle for final output
        auto backbuffer = m_render_graph->get_backbuffer_handle();

        // Depth pre-pass (if enabled)
        if (m_config.renderer.forward_plus.enable_depth_prepass) {
            builder.add_graphics_pass(
                        "depth_prepass",
                        [this](const batleth::PassExecutionContext &ctx) {
                            if (!m_active_scene) return;

                            // Create frame info
                            FrameInfo frame_info{
                                static_cast<int>(ctx.frame_index),
                                ctx.delta_time,
                                ctx.command_buffer,
                                m_active_scene->get_camera(),
                                m_global_descriptor_sets[ctx.frame_index],
                                m_texture_manager->get_descriptor_set(),
                                m_active_scene->get_game_objects()
                            };

                            // Render depth only for opaque geometry
                            m_depth_prepass_system->render(frame_info, RenderMode::OpaqueOnly);
                        }
                    )
                    .set_depth_attachment(depth_buffer, VK_ATTACHMENT_LOAD_OP_CLEAR, {1.0f, 0})
                    .write(depth_buffer, batleth::ResourceUsage::DepthStencilWrite);
        }

        // Light culling compute pass (Forward+ core)
        if (m_config.renderer.forward_plus.enabled && m_config.renderer.forward_plus.enable_depth_prepass) {
            builder.add_compute_pass(
                        "light_culling",
                        [this, depth_buffer, light_grid, light_count, tile_count_x, tile_count_y, tile_size, max_lights_per_tile, extent](
                            const batleth::PassExecutionContext &ctx
                        ) {
                            if (!m_active_scene) return;
                            if (m_light_culling_pipeline == VK_NULL_HANDLE) return;

                            // Get render graph resources
                            VkImageView depth_view = ctx.get_image_view(depth_buffer);
                            VkBuffer light_grid_buffer = ctx.get_buffer(light_grid);
                            VkBuffer light_count_buffer = ctx.get_buffer(light_count);

                            // Allocate/update descriptor set for this frame if needed
                            // For now, we'll allocate descriptor sets lazily
                            if (m_forward_plus_descriptor_sets.size() < MAX_FRAMES_IN_FLIGHT) {
                                m_forward_plus_descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);

                                for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                                    if (!batleth::DescriptorWriter(*m_forward_plus_set_layout, *m_forward_plus_descriptor_pool)
                                            .build(m_forward_plus_descriptor_sets[i])) {
                                        FED_ERROR("Failed to allocate Forward+ descriptor set for frame {}", i);
                                        return;
                                    }
                                }
                            }

                            // Write descriptor set with current resources
                            VkDescriptorImageInfo depth_image_info{};
                            depth_image_info.sampler = m_depth_sampler;
                            depth_image_info.imageView = depth_view;
                            depth_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                            VkDescriptorBufferInfo light_grid_info{};
                            light_grid_info.buffer = light_grid_buffer;
                            light_grid_info.offset = 0;
                            light_grid_info.range = VK_WHOLE_SIZE;

                            VkDescriptorBufferInfo light_count_info{};
                            light_count_info.buffer = light_count_buffer;
                            light_count_info.offset = 0;
                            light_count_info.range = VK_WHOLE_SIZE;

                            batleth::DescriptorWriter(*m_forward_plus_set_layout, *m_forward_plus_descriptor_pool)
                                    .write_image(0, &depth_image_info)
                                    .write_buffer(1, &light_grid_info)
                                    .write_buffer(2, &light_count_info)
                                    .overwrite(m_forward_plus_descriptor_sets[ctx.frame_index]);

                            // Bind compute pipeline
                            ::vkCmdBindPipeline(ctx.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_light_culling_pipeline);

                            // Bind descriptor sets
                            VkDescriptorSet descriptor_sets[] = {
                                m_global_descriptor_sets[ctx.frame_index],       // Set 0: Global UBO
                                m_forward_plus_descriptor_sets[ctx.frame_index]  // Set 1: Forward+ resources
                            };

                            ::vkCmdBindDescriptorSets(
                                ctx.command_buffer,
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                m_light_culling_pipeline_layout,
                                0,
                                2,
                                descriptor_sets,
                                0,
                                nullptr
                            );

                            // Calculate push constants
                            auto &camera = m_active_scene->get_camera();
                            auto view_projection = camera.get_projection() * camera.get_view();
                            auto view_projection_inverse = glm::inverse(view_projection);

                            LightCullingPushConstants push_constants{};
                            push_constants.view_projection_inverse = view_projection_inverse;
                            push_constants.screen_size = glm::uvec2(extent.width, extent.height);
                            push_constants.tile_count = glm::uvec2(tile_count_x, tile_count_y);
                            push_constants.num_lights = static_cast<uint32_t>(m_current_ubo.num_lights);
                            push_constants.tile_size = tile_size;
                            push_constants.z_near = 0.1f;  // TODO: Get from camera
                            push_constants.z_far = 100.0f; // TODO: Get from camera

                            ::vkCmdPushConstants(
                                ctx.command_buffer,
                                m_light_culling_pipeline_layout,
                                VK_SHADER_STAGE_COMPUTE_BIT,
                                0,
                                sizeof(LightCullingPushConstants),
                                &push_constants
                            );

                            // Dispatch compute shader
                            // Workgroup size is 16x16, so we need (tile_count_x, tile_count_y, 1) workgroups
                            ::vkCmdDispatch(ctx.command_buffer, tile_count_x, tile_count_y, 1);

                            // FED_TRACE("Light culling compute dispatched: {}x{} tiles, {} lights",
                            //           tile_count_x, tile_count_y, push_constants.num_lights);
                        }
                    )
                    .read(depth_buffer, batleth::ResourceUsage::SampledImage)
                    .write(light_grid, batleth::ResourceUsage::StorageBufferWrite)
                    .write(light_count, batleth::ResourceUsage::StorageBufferWrite);
        }

        // Main geometry/shading pass
        builder.add_graphics_pass(
                    "forward_shading",
                    [this, tile_count_x, tile_count_y, tile_size, max_lights_per_tile](
                        const batleth::PassExecutionContext &ctx
                    ) {
                        if (!m_active_scene) return;

                        // Set Forward+ resources if enabled
                        if (m_config.renderer.forward_plus.enabled &&
                            !m_forward_plus_descriptor_sets.empty()) {
                            m_simple_render_system->set_forward_plus_resources(
                                m_forward_plus_descriptor_sets[ctx.frame_index],
                                tile_count_x,
                                tile_count_y,
                                tile_size,
                                max_lights_per_tile
                            );
                        }

                        // Create frame info
                        FrameInfo frame_info{
                            static_cast<int>(ctx.frame_index),
                            ctx.delta_time,
                            ctx.command_buffer,
                            m_active_scene->get_camera(),
                            m_global_descriptor_sets[ctx.frame_index],
                            m_texture_manager->get_descriptor_set(),
                            m_active_scene->get_game_objects()
                        };

                        // Render opaque game objects only
                        m_simple_render_system->render(frame_info, RenderMode::OpaqueOnly);

                        if (m_debug_rendering_enabled) {
                            // Render point lights
                            m_point_light_system->render(frame_info);
                        }

                        // Render custom systems
                        for (auto &system: m_custom_render_systems) {
                            system->render(frame_info);
                        }
                    }
                )
                .set_color_attachment(0, color_target, VK_ATTACHMENT_LOAD_OP_CLEAR, {{0.01f, 0.01f, 0.01f, 1.0f}})
                .set_depth_attachment(
                    depth_buffer,
                    m_config.renderer.forward_plus.enable_depth_prepass
                        ? VK_ATTACHMENT_LOAD_OP_LOAD   // Keep existing depth from pre-pass
                        : VK_ATTACHMENT_LOAD_OP_CLEAR,  // Clear if no pre-pass
                    {1.0f, 0}
                )
                .write(color_target, batleth::ResourceUsage::ColorAttachment);

        // Depth buffer is always written as an attachment (even if pipeline disables writes)
        // The render graph needs to know about the depth attachment
        builder.write(depth_buffer, batleth::ResourceUsage::DepthStencilWrite);

        // Forward+ resources (read light grid and count)
        if (m_config.renderer.forward_plus.enabled) {
            builder.read(light_grid, batleth::ResourceUsage::StorageBufferRead)
                   .read(light_count, batleth::ResourceUsage::StorageBufferRead);
        }

        // NEW: Transparency pass - render transparent objects after opaque
        builder.add_graphics_pass(
                    "transparency_pass",
                    [this, tile_count_x, tile_count_y, tile_size, max_lights_per_tile](const batleth::PassExecutionContext& ctx) {
                        if (!m_active_scene) return;

                        // Set Forward+ resources if enabled
                        if (m_config.renderer.forward_plus.enabled) {
                            m_simple_render_system->set_forward_plus_resources(
                                m_forward_plus_descriptor_sets[ctx.frame_index],
                                tile_count_x,
                                tile_count_y,
                                tile_size,
                                max_lights_per_tile
                            );
                        }

                        // Create frame info (same as forward shading)
                        FrameInfo frame_info{
                            static_cast<int>(ctx.frame_index),
                            ctx.delta_time,
                            ctx.command_buffer,
                            m_active_scene->get_camera(),
                            m_global_descriptor_sets[ctx.frame_index],
                            m_texture_manager->get_descriptor_set(),
                            m_active_scene->get_game_objects()
                        };

                        // Render ONLY transparent objects, sorted back-to-front
                        m_simple_render_system->render(frame_info, RenderMode::TransparentOnly);
                    }
                )
                .set_color_attachment(
                    0,
                    color_target,
                    VK_ATTACHMENT_LOAD_OP_LOAD,  // LOAD - preserve opaque rendering
                    {0.0f, 0.0f, 0.0f, 1.0f}     // Clear value unused (we LOAD)
                )
                .set_depth_attachment(
                    depth_buffer,
                    VK_ATTACHMENT_LOAD_OP_LOAD,  // LOAD - preserve opaque depth
                    {1.0f, 0}
                )
                .read(depth_buffer, batleth::ResourceUsage::DepthStencilRead)  // Read-only depth
                .write(color_target, batleth::ResourceUsage::ColorAttachment);

        // Forward+ resources (read in transparency pass too)
        if (m_config.renderer.forward_plus.enabled) {
            builder.read(light_grid, batleth::ResourceUsage::StorageBufferRead)
                   .read(light_count, batleth::ResourceUsage::StorageBufferRead);
        }

        // Blit offscreen to backbuffer (if offscreen rendering is enabled)
        if (m_config.renderer.offscreen.enabled) {
            builder.add_graphics_pass(
                        "blit_to_backbuffer",
                        [this, color_target](const batleth::PassExecutionContext &ctx) {
                            // Get the offscreen color buffer image view
                            VkImageView offscreen_view = ctx.get_image_view(color_target);

                            // Render fullscreen blit
                            m_blit_render_system->render(ctx.command_buffer, offscreen_view, m_offscreen_sampler, ctx.frame_index);
                        }
                    )
                    .read(color_target, batleth::ResourceUsage::SampledImage)
                    .set_color_attachment(0, backbuffer, VK_ATTACHMENT_LOAD_OP_CLEAR)
                    .write(backbuffer, batleth::ResourceUsage::ColorAttachment);
        }

        // ImGui pass (if enabled)
        if (m_imgui_context) {
            builder.add_graphics_pass(
                        "imgui",
                        [this](const batleth::PassExecutionContext &ctx) {
                            m_imgui_context->render(ctx.command_buffer);
                        }
                    )
                    .set_color_attachment(
                        0,
                        backbuffer,
                        VK_ATTACHMENT_LOAD_OP_LOAD  // Keep previous rendering
                    )
                    .read(backbuffer, batleth::ResourceUsage::ColorAttachment)
                    .write(backbuffer, batleth::ResourceUsage::ColorAttachment);
        }

        m_render_graph->compile();
        m_last_render_extent = extent;

        // Update offscreen image view handle for viewport access
        if (m_config.renderer.offscreen.enabled && m_offscreen_color_handle != batleth::INVALID_RESOURCE) {
            m_offscreen_image_view = m_render_graph->get_image_view(m_offscreen_color_handle);
            FED_DEBUG("Offscreen image view updated for viewport: {}", (void*)m_offscreen_image_view);
        }

        FED_INFO("Forward+ render graph compiled with {} passes", m_render_graph->get_pass_count());
    }

    auto Renderer::should_rebuild_render_graph() const -> bool {
        if (!m_render_graph) return true;

        auto current_extent = m_swapchain->get_extent();
        if (current_extent.width != m_last_render_extent.width ||
            current_extent.height != m_last_render_extent.height) {
            return true;
        }

        return false;
    }

    auto Renderer::invalidate_render_graph() -> void {
        FED_INFO("Render graph invalidated - will rebuild on next frame");
        m_render_graph.reset();
    }

    auto Renderer::set_debug_rendering_enabled(bool enabled) -> void {
        m_debug_rendering_enabled = enabled;
    }

    auto Renderer::is_debug_rendering_enabled() const -> bool {
        return m_debug_rendering_enabled;
    }

    auto Renderer::set_imgui_callback(ImGuiCallback callback) -> void {
        m_imgui_callback = std::move(callback);
    }

    auto Renderer::create_imgui_viewport_texture() -> void* {
        if (!m_config.renderer.offscreen.enabled) {
            FED_WARN("Offscreen rendering is not enabled - cannot create viewport texture");
            return nullptr;
        }

        if (m_offscreen_image_view == VK_NULL_HANDLE || m_offscreen_sampler == VK_NULL_HANDLE) {
            FED_WARN("Offscreen image view or sampler not available");
            return nullptr;
        }

        // Create ImGui descriptor set for the offscreen texture
        // ImGui_ImplVulkan_AddTexture returns an ImTextureID (VkDescriptorSet)
        VkDescriptorSet descriptor_set = ::ImGui_ImplVulkan_AddTexture(
            m_offscreen_sampler,
            m_offscreen_image_view,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

        FED_DEBUG("Created ImGui viewport texture descriptor set: {}", (void*)descriptor_set);
        return descriptor_set;
    }

    auto Renderer::get_allocator() -> VmaAllocator  {

        if (m_allocator) return m_allocator->get_vma_allocator();

        // Create transient allocator
        batleth::TransientAllocator::Config alloc_config{};
        alloc_config.instance = get_instance();
        alloc_config.physical_device = get_physical_device();
        alloc_config.device = get_device();

        m_allocator = std::make_unique<batleth::TransientAllocator>(alloc_config);

        return m_allocator->get_vma_allocator();
    }

    auto Renderer::render_scene(Scene *scene, float delta_time) -> void {
        if (!scene) {
            FED_WARN("render_scene called with null scene");
            return;
        }

        m_active_scene = scene;

        // Rebuild render graph if needed
        if (should_rebuild_render_graph()) {
            FED_INFO("Rebuilding render graph due to resize or invalidation");
            build_default_render_graph();
        }

        // Begin frame (acquires swapchain image and begins ImGui frame)
        if (!begin_frame()) {
            return; // Frame not ready (e.g., window minimized)
        }

        // Call ImGui callback if enabled (after begin_frame() has started ImGui frame)
        if (m_imgui_context && m_imgui_callback) {
            m_imgui_callback();
            m_imgui_context->end_frame();
        }

        // Update camera matrices from scene
        update_camera_from_scene(scene, delta_time);

        // Update global UBO from scene
        update_global_ubo(scene, delta_time);

        // Begin command buffer
        auto cmd = get_current_command_buffer();
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = 0;
        ::vkBeginCommandBuffer(cmd, &begin_info);

        // Set backbuffer with current swapchain image
        m_render_graph->set_backbuffer(
            m_swapchain->get_images()[m_current_image_index],
            m_swapchain->get_image_views()[m_current_image_index],
            m_swapchain->get_format(),
            m_swapchain->get_extent()
        );

        // Execute render graph
        m_render_graph->execute(cmd, m_current_frame, delta_time);

        // End command buffer
        ::vkEndCommandBuffer(cmd);

        // End frame (submits command buffer, presents image)
        end_frame();
    }
} // namespace klingon
