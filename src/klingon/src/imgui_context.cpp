#include "klingon/imgui_context.hpp"
#include "batleth/device.hpp"
#include "batleth/descriptors.hpp"
#include "federation/log.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <cstring>

namespace klingon {

ImGuiContext::ImGuiContext(GLFWwindow* window,
                           batleth::Device& device,
                           VkFormat color_format,
                           std::uint32_t image_count)
    : ImGuiContext(window, device, color_format, image_count, Config{}) {
}

ImGuiContext::ImGuiContext(GLFWwindow* window,
                           batleth::Device& device,
                           VkFormat color_format,
                           std::uint32_t image_count,
                           const Config& config)
    : m_device(&device)
    , m_config(config) {

    FED_INFO("Initializing ImGui context with dynamic rendering");

    // Create ImGui context
    IMGUI_CHECKVERSION();
    ::ImGui::CreateContext();
    ImGuiIO& io = ::ImGui::GetIO();

    // Configure ImGui
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    if (config.enable_docking) {
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        FED_DEBUG("ImGui docking enabled");
    }

    if (config.enable_viewports) {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        FED_DEBUG("ImGui viewports enabled");
    }

    // Setup style
    setup_style();

    // Create descriptor pool for ImGui using our DescriptorPool builder
    m_descriptor_pool = batleth::DescriptorPool::Builder(device.get_logical_device())
        .set_max_sets(1000)
        .add_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000)
        .set_pool_flags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
        .build();

    FED_DEBUG("Created ImGui descriptor pool");

    // Initialize GLFW backend
    if (!ImGui_ImplGlfw_InitForVulkan(window, true)) {
        throw std::runtime_error("Failed to initialize ImGui GLFW backend");
    }

    // Initialize Vulkan backend with dynamic rendering
    ImGui_ImplVulkan_InitInfo init_info{};
    std::memset(&init_info, 0, sizeof(init_info));

    init_info.ApiVersion = VK_API_VERSION_1_3;
    init_info.Instance = device.get_instance();
    init_info.PhysicalDevice = device.get_physical_device();
    init_info.Device = device.get_logical_device();
    init_info.QueueFamily = device.get_graphics_queue_family();
    init_info.Queue = device.get_graphics_queue();
    init_info.DescriptorPool = m_descriptor_pool->get_pool();
    init_info.MinImageCount = image_count;
    init_info.ImageCount = image_count;
    init_info.UseDynamicRendering = true;

    // Set up dynamic rendering info
    VkPipelineRenderingCreateInfoKHR pipeline_rendering_create_info{};
    pipeline_rendering_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipeline_rendering_create_info.colorAttachmentCount = 1;
    pipeline_rendering_create_info.pColorAttachmentFormats = &color_format;

    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = pipeline_rendering_create_info;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        ImGui_ImplGlfw_Shutdown();
        throw std::runtime_error("Failed to initialize ImGui Vulkan backend");
    }

    FED_INFO("ImGui context initialized successfully");
}

ImGuiContext::~ImGuiContext() {
    FED_INFO("Shutting down ImGui context");

    // Wait for device to be idle before cleanup
    vkDeviceWaitIdle(m_device->get_logical_device());

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ::ImGui::DestroyContext();

    // DescriptorPool will be destroyed automatically (unique_ptr)
}

auto ImGuiContext::begin_frame() -> void {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ::ImGui::NewFrame();
}

auto ImGuiContext::end_frame() -> void {
    ::ImGui::Render();
}

auto ImGuiContext::render(VkCommandBuffer command_buffer) -> void {
    ImGui_ImplVulkan_RenderDrawData(::ImGui::GetDrawData(), command_buffer);

    // Update and render additional platform windows (if multi-viewport is enabled)
    if (::ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ::ImGui::UpdatePlatformWindows();
        ::ImGui::RenderPlatformWindowsDefault();
    }
}

auto ImGuiContext::on_resize(std::uint32_t width, std::uint32_t height) -> void {
    (void)width;
    (void)height;
    // ImGui handles resize automatically through GLFW callbacks
}

auto ImGuiContext::setup_style() -> void {
    // Use dark theme as default
    ::ImGui::StyleColorsDark();

    // Customize style for better editor look
    ImGuiStyle& style = ::ImGui::GetStyle();

    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.TabRounding = 0.0f;

    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;

    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(4.0f, 3.0f);
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);

    // Color scheme - darker theme
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

    FED_DEBUG("ImGui style configured");
}

} // namespace klingon
