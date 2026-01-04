#include "klingon/renderer.hpp"
#include "klingon/imgui_context.hpp"
#include "borg/window.hpp"
#include "batleth/instance.hpp"
#include "batleth/device.hpp"
#include "batleth/swapchain.hpp"
#include "batleth/shader.hpp"
#include "batleth/pipeline.hpp"
#include "federation/log.hpp"

#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>

namespace klingon {

Renderer::Renderer(const Config& config, borg::Window& window)
    : m_window(window), m_config(config) {
    FED_INFO("Initializing renderer");

    create_instance();
    create_device();
    create_swapchain();
    create_shaders();
    create_pipeline();
    create_command_pool();
    create_command_buffers();
    create_sync_objects();

    // Initialize ImGui if requested
    if (m_config.enable_imgui) {
        FED_INFO("Initializing ImGui for editor");
        m_imgui_context = std::make_unique<ImGuiContext>(
            m_window.get_native_handle(),
            *m_device,
            m_swapchain->get_format(),
            static_cast<std::uint32_t>(m_swapchain->get_image_count())
        );
    }

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
    for (auto semaphore : m_image_available_semaphores) {
        ::vkDestroySemaphore(m_device->get_logical_device(), semaphore, nullptr);
    }
    for (auto semaphore : m_render_finished_semaphores) {
        ::vkDestroySemaphore(m_device->get_logical_device(), semaphore, nullptr);
    }
    for (auto fence : m_in_flight_fences) {
        ::vkDestroyFence(m_device->get_logical_device(), fence, nullptr);
    }

    if (m_command_pool != VK_NULL_HANDLE) {
        ::vkDestroyCommandPool(m_device->get_logical_device(), m_command_pool, nullptr);
    }

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

auto Renderer::end_frame() -> void {
    // End ImGui frame (prepares draw data)
    if (m_imgui_context) {
        m_imgui_context->end_frame();
    }

    // Begin recording command buffer
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = 0;
    begin_info.pInheritanceInfo = nullptr;

    if (::vkBeginCommandBuffer(m_command_buffers[m_current_frame], &begin_info) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer");
    }

    // Record rendering commands (includes triangle + ImGui)
    record_command_buffer(m_command_buffers[m_current_frame], m_current_image_index);

    // End command buffer recording
    if (::vkEndCommandBuffer(m_command_buffers[m_current_frame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
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

    if (::vkQueueSubmit(m_device->get_graphics_queue(), 1, &submit_info, m_in_flight_fences[m_current_frame]) != VK_SUCCESS) {
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

auto Renderer::on_resize() -> void {
    FED_DEBUG("Window resized, marking for swapchain recreation");
    m_framebuffer_resized = true;
}

auto Renderer::recreate_swapchain() -> void {
    FED_INFO("Recreating swapchain");

    // Wait for device to be idle
    m_device->wait_idle();

    // Cleanup old swapchain-dependent resources
    m_pipeline.reset();
    m_swapchain.reset();

    // Recreate swapchain and dependent resources
    create_swapchain();
    create_pipeline();

    FED_INFO("Swapchain recreated successfully");
}

auto Renderer::create_instance() -> void {
    FED_DEBUG("Creating Vulkan instance");

    // FED_DEBUG("GLFW Initialized: {}", static_cast<bool>(glfwInit()));

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
    for (const auto& extension : available_extensions) {
        FED_DEBUG(" - {} (version {})", extension.extensionName, extension.specVersion);
    }

    // Get required GLFW extensions
    std::uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = ::glfwGetRequiredInstanceExtensions(&glfw_extension_count);

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
    if (m_config.enable_validation) {
        extensions.push_back("VK_EXT_debug_utils");
        FED_DEBUG("  - VK_EXT_debug_utils (for validation)");
    }

    batleth::Instance::Config instance_config{};
    instance_config.application_name = m_config.application_name;
    instance_config.application_version = m_config.application_version;
    instance_config.engine_name = "Klingon Engine";
    instance_config.engine_version = 1;
    instance_config.extensions = extensions;
    instance_config.enable_validation = m_config.enable_validation;

    if (m_config.enable_validation) {
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
    device_config.device_extensions = {"VK_KHR_swapchain"};

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

    if (::vkAllocateCommandBuffers(m_device->get_logical_device(), &alloc_info, m_command_buffers.data()) != VK_SUCCESS) {
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
        if (::vkCreateSemaphore(m_device->get_logical_device(), &semaphore_info, nullptr, &m_image_available_semaphores[i]) != VK_SUCCESS ||
            ::vkCreateSemaphore(m_device->get_logical_device(), &semaphore_info, nullptr, &m_render_finished_semaphores[i]) != VK_SUCCESS ||
            ::vkCreateFence(m_device->get_logical_device(), &fence_info, nullptr, &m_in_flight_fences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects");
        }
    }

    FED_DEBUG("Synchronization objects created successfully");
}

auto Renderer::create_shaders() -> void {
    FED_DEBUG("Creating shaders");

    // Load vertex shader (GLSL will be compiled at runtime)
    batleth::Shader::Config vertex_config{};
    vertex_config.device = m_device->get_logical_device();
    vertex_config.filepath = "shaders/triangle.vert";  // Changed to .vert for GLSL
    vertex_config.stage = batleth::Shader::Stage::Vertex;
    vertex_config.enable_hot_reload = true;

    m_shaders.push_back(std::make_unique<batleth::Shader>(vertex_config));

    // Load fragment shader (GLSL will be compiled at runtime)
    batleth::Shader::Config fragment_config{};
    fragment_config.device = m_device->get_logical_device();
    fragment_config.filepath = "shaders/triangle.frag";  // Changed to .frag for GLSL
    fragment_config.stage = batleth::Shader::Stage::Fragment;
    vertex_config.enable_hot_reload = true;

    m_shaders.push_back(std::make_unique<batleth::Shader>(fragment_config));

    FED_DEBUG("Loaded {} shader stages", m_shaders.size());
}

auto Renderer::create_pipeline() -> void {
    FED_DEBUG("Creating graphics pipeline");

    // Convert unique_ptr vector to raw pointer vector for Pipeline::Config
    std::vector<batleth::Shader*> shader_ptrs;
    shader_ptrs.reserve(m_shaders.size());
    for (const auto& shader : m_shaders) {
        shader_ptrs.push_back(shader.get());
    }

    batleth::Pipeline::Config config{};
    config.device = m_device->get_logical_device();
    config.color_format = m_swapchain->get_format();  // Use dynamic rendering
    config.viewport_extent = m_swapchain->get_extent();
    config.shaders = shader_ptrs;
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.polygon_mode = VK_POLYGON_MODE_FILL;
    config.cull_mode = VK_CULL_MODE_BACK_BIT;
    config.front_face = VK_FRONT_FACE_CLOCKWISE;

    m_pipeline = std::make_unique<batleth::Pipeline>(config);
}

auto Renderer::record_command_buffer(VkCommandBuffer command_buffer, std::uint32_t image_index) -> void {
    // Transition image to color attachment optimal
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_swapchain->get_images()[image_index];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    ::vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    // Begin dynamic rendering
    VkRenderingAttachmentInfo color_attachment{};
    color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attachment.imageView = m_swapchain->get_image_views()[image_index];
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.clearValue.color = {{0.39f, 0.58f, 0.93f, 1.0f}};

    VkRenderingInfo rendering_info{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea.offset = {0, 0};
    rendering_info.renderArea.extent = m_swapchain->get_extent();
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;

    ::vkCmdBeginRendering(command_buffer, &rendering_info);

    // Bind the graphics pipeline
    ::vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->get_handle());

    // Set viewport
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapchain->get_extent().width);
    viewport.height = static_cast<float>(m_swapchain->get_extent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    ::vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    // Set scissor
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapchain->get_extent();
    ::vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    // Draw the triangle (3 vertices, 1 instance, first vertex 0, first instance 0)
    ::vkCmdDraw(command_buffer, 3, 1, 0, 0);

    // Render ImGui if enabled
    if (m_imgui_context) {
        m_imgui_context->render(command_buffer);
    }

    // End dynamic rendering
    ::vkCmdEndRendering(command_buffer);

    // Transition image to present
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = 0;

    ::vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

} // namespace klingon
