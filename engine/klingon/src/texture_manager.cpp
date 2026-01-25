#include "klingon/texture_manager.hpp"
#include "batleth/image_utils.hpp"
#include "federation/log.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <filesystem>
#include <stdexcept>

namespace klingon {

TextureManager::TextureManager(const Config& config)
    : m_device(config.device)
    , m_allocator(config.allocator)
    , m_max_materials(config.max_materials)
    , m_max_textures(config.max_textures) {

    FED_INFO("Initializing TextureManager (max_textures: {}, max_materials: {})",
             m_max_textures, m_max_materials);

    // Create default sampler
    batleth::Sampler::Config sampler_config{};
    sampler_config.device = m_device.get_logical_device();
    sampler_config.mag_filter = VK_FILTER_LINEAR;
    sampler_config.min_filter = VK_FILTER_LINEAR;
    sampler_config.mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_config.address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_config.address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_config.address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_config.anisotropy_enable = true;
    sampler_config.max_anisotropy = 16.0f;
    sampler_config.max_lod = VK_LOD_CLAMP_NONE;

    m_default_sampler = std::make_unique<batleth::Sampler>(sampler_config);

    // Reserve space for textures
    m_albedo_textures.reserve(m_max_textures);
    m_normal_textures.reserve(m_max_textures);
    m_pbr_textures.reserve(m_max_textures);

    // Reserve space for materials
    m_material_data.reserve(m_max_materials);

    // Create default textures (indices 0, 1, 2)
    create_default_textures();

    // Create material buffer
    create_material_buffer();

    // Create descriptor resources
    create_descriptor_set_layout();
    create_descriptor_pool();
    create_descriptor_set();
    update_descriptors();

    FED_INFO("TextureManager initialized successfully");
}

auto TextureManager::create_default_textures() -> void {
    FED_TRACE("Creating default textures");

    // Helper to create 1x1 texture
    auto create_1x1_texture = [&](uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                                   batleth::TextureType type,
                                   const std::string& name) -> std::unique_ptr<batleth::Texture> {
        // Create 1x1 image
        batleth::Image::Config img_config{};
        img_config.device = m_device.get_logical_device();
        img_config.allocator = m_allocator;
        img_config.width = 1;
        img_config.height = 1;
        img_config.mip_levels = 1;
        img_config.format = VK_FORMAT_R8G8B8A8_UNORM;  // Use UNORM for default textures
        img_config.tiling = VK_IMAGE_TILING_OPTIMAL;
        img_config.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        img_config.aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
        img_config.create_view = true;

        auto image = std::make_unique<batleth::Image>(img_config);

        // Upload pixel data via staging buffer
        uint8_t pixel_data[4] = {r, g, b, a};

        batleth::Buffer staging_buffer(
            m_device,
            4,
            1,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        staging_buffer.map();
        staging_buffer.write_to_buffer(pixel_data);
        staging_buffer.unmap();

        // Copy staging buffer to image (need command buffer)
        // For now, we'll use a simple approach with immediate command buffer submission
        // In production, this should be part of a proper upload queue

        VkCommandBuffer cmd = m_device.begin_single_time_commands();

        // Transition to TRANSFER_DST
        image->transition_layout(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // Copy buffer to image
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {1, 1, 1};

        ::vkCmdCopyBufferToImage(cmd, staging_buffer.get_buffer(), image->get_image(),
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        // Transition to SHADER_READ_ONLY
        image->transition_layout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        m_device.end_single_time_commands(cmd);

        return std::make_unique<batleth::Texture>(std::move(image), type, name);
    };

    // Create default albedo (white)
    m_albedo_textures.push_back(create_1x1_texture(255, 255, 255, 255,
                                                   batleth::TextureType::Albedo,
                                                   "default_albedo"));

    // Create default normal (flat normal: 128, 128, 255 = (0, 0, 1) in tangent space)
    m_normal_textures.push_back(create_1x1_texture(128, 128, 255, 255,
                                                   batleth::TextureType::Normal,
                                                   "default_normal"));

    // Create default PBR (metallic=1.0, roughness=0.5: 255, 128, 0, 255)
    m_pbr_textures.push_back(create_1x1_texture(255, 128, 0, 255,
                                                batleth::TextureType::MetallicRoughness,
                                                "default_pbr"));

    FED_TRACE("Created 3 default textures");
}

auto TextureManager::create_material_buffer() -> void {
    FED_TRACE("Creating material buffer ({} materials, {} bytes)",
             m_max_materials, m_max_materials * sizeof(MaterialGPU));

    m_material_buffer = std::make_unique<batleth::Buffer>(
        m_device,
        sizeof(MaterialGPU),
        m_max_materials,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    // Create and upload default material at index 0
    MaterialGPU default_material{};
    default_material.base_color_factor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    default_material.metallic_factor = 0.0f;
    default_material.roughness_factor = 0.5f;
    default_material.normal_scale = 1.0f;
    default_material.albedo_texture_index = 0;  // Default white texture
    default_material.normal_texture_index = 1;  // Default flat normal
    default_material.pbr_texture_index = 2;     // Default PBR
    default_material.material_flags = 0;        // No textures (use factors only)

    m_material_data.push_back(default_material);
    m_material_count = 1;

    // Upload default material to GPU
    batleth::Buffer staging_buffer(
        m_device,
        sizeof(MaterialGPU),
        1,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    staging_buffer.map();
    staging_buffer.write_to_buffer(&default_material);
    staging_buffer.unmap();

    VkCommandBuffer cmd = m_device.begin_single_time_commands();

    VkBufferCopy copy_region{};
    copy_region.srcOffset = 0;
    copy_region.dstOffset = 0;
    copy_region.size = sizeof(MaterialGPU);

    ::vkCmdCopyBuffer(cmd, staging_buffer.get_buffer(), m_material_buffer->get_buffer(), 1, &copy_region);

    m_device.end_single_time_commands(cmd);

    FED_TRACE("Material buffer created with default material at index 0");
}

auto TextureManager::create_descriptor_set_layout() -> void {
    FED_TRACE("Creating descriptor set layout");

    batleth::DescriptorSetLayout::Builder layout_builder(m_device.get_logical_device());

    // Binding 0: Albedo textures (unbounded array)
    layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_FRAGMENT_BIT, m_max_textures);

    // Binding 1: Normal textures (unbounded array)
    layout_builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_FRAGMENT_BIT, m_max_textures);

    // Binding 2: PBR textures (unbounded array)
    layout_builder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_FRAGMENT_BIT, m_max_textures);

    // Binding 3: Material buffer (SSBO)
    layout_builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 1);

    m_descriptor_layout = layout_builder.build();

    FED_TRACE("Descriptor set layout created");
}

auto TextureManager::create_descriptor_pool() -> void {
    FED_TRACE("Creating descriptor pool");

    batleth::DescriptorPool::Builder pool_builder(m_device.get_logical_device());

    pool_builder.set_max_sets(1);
    pool_builder.add_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_max_textures * 3);
    pool_builder.add_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);

    m_descriptor_pool = pool_builder.build();

    FED_TRACE("Descriptor pool created");
}

auto TextureManager::create_descriptor_set() -> void {
    FED_TRACE("Allocating descriptor set");

    if (!m_descriptor_pool->allocate_descriptor_set(m_descriptor_layout->get_layout(), m_descriptor_set)) {
        FED_FATAL("Failed to allocate descriptor set");
        throw std::runtime_error("Failed to allocate descriptor set");
    }

    FED_TRACE("Descriptor set allocated");
}

auto TextureManager::get_descriptor_layout() const -> VkDescriptorSetLayout {
    return m_descriptor_layout->get_layout();
}

auto TextureManager::update_descriptors() -> void {
    if (!m_descriptors_dirty) {
        return;
    }

    FED_TRACE("Updating bindless descriptors");

    std::vector<VkDescriptorImageInfo> albedo_infos;
    std::vector<VkDescriptorImageInfo> normal_infos;
    std::vector<VkDescriptorImageInfo> pbr_infos;

    // Build image info arrays
    for (const auto& tex : m_albedo_textures) {
        VkDescriptorImageInfo info{};
        info.sampler = m_default_sampler->get_handle();
        info.imageView = tex->get_image().get_view();
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        albedo_infos.push_back(info);
    }

    for (const auto& tex : m_normal_textures) {
        VkDescriptorImageInfo info{};
        info.sampler = m_default_sampler->get_handle();
        info.imageView = tex->get_image().get_view();
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        normal_infos.push_back(info);
    }

    for (const auto& tex : m_pbr_textures) {
        VkDescriptorImageInfo info{};
        info.sampler = m_default_sampler->get_handle();
        info.imageView = tex->get_image().get_view();
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        pbr_infos.push_back(info);
    }

    // Material buffer info
    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = m_material_buffer->get_buffer();
    buffer_info.offset = 0;
    buffer_info.range = VK_WHOLE_SIZE;

    // Write descriptor updates
    std::vector<VkWriteDescriptorSet> writes;

    // Albedo textures
    VkWriteDescriptorSet albedo_write{};
    albedo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    albedo_write.dstSet = m_descriptor_set;
    albedo_write.dstBinding = 0;
    albedo_write.dstArrayElement = 0;
    albedo_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    albedo_write.descriptorCount = static_cast<uint32_t>(albedo_infos.size());
    albedo_write.pImageInfo = albedo_infos.data();
    writes.push_back(albedo_write);

    // Normal textures
    VkWriteDescriptorSet normal_write{};
    normal_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    normal_write.dstSet = m_descriptor_set;
    normal_write.dstBinding = 1;
    normal_write.dstArrayElement = 0;
    normal_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    normal_write.descriptorCount = static_cast<uint32_t>(normal_infos.size());
    normal_write.pImageInfo = normal_infos.data();
    writes.push_back(normal_write);

    // PBR textures
    VkWriteDescriptorSet pbr_write{};
    pbr_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    pbr_write.dstSet = m_descriptor_set;
    pbr_write.dstBinding = 2;
    pbr_write.dstArrayElement = 0;
    pbr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pbr_write.descriptorCount = static_cast<uint32_t>(pbr_infos.size());
    pbr_write.pImageInfo = pbr_infos.data();
    writes.push_back(pbr_write);

    // Material buffer
    VkWriteDescriptorSet buffer_write{};
    buffer_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    buffer_write.dstSet = m_descriptor_set;
    buffer_write.dstBinding = 3;
    buffer_write.dstArrayElement = 0;
    buffer_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    buffer_write.descriptorCount = 1;
    buffer_write.pBufferInfo = &buffer_info;
    writes.push_back(buffer_write);

    ::vkUpdateDescriptorSets(m_device.get_logical_device(),
                            static_cast<uint32_t>(writes.size()),
                            writes.data(),
                            0, nullptr);

    m_descriptors_dirty = false;

    FED_TRACE("Descriptors updated ({} albedo, {} normal, {} pbr textures, {} materials)",
             albedo_infos.size(), normal_infos.size(), pbr_infos.size(), m_material_count);
}

auto TextureManager::load_texture(
    const std::string& filepath,
    batleth::TextureType type,
    bool generate_mipmaps
) -> uint32_t {

    std::filesystem::path path(m_textures_dir + "/" + filepath);

    // Check cache first
    std::unordered_map<std::string, uint32_t>* cache = nullptr;
    switch (type) {
        case batleth::TextureType::Albedo:
            cache = &m_albedo_cache;
            break;
        case batleth::TextureType::Normal:
            cache = &m_normal_cache;
            break;
        case batleth::TextureType::MetallicRoughness:
            cache = &m_pbr_cache;
            break;
        default:
            FED_ERROR("Unknown texture type");
            return 0;  // Return default
    }

    auto it = cache->find(filepath);
    if (it != cache->end()) {
        FED_TRACE("Texture already loaded: {}", filepath);
        return it->second;
    }

    // Detect format by extension
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    uint32_t index = 0;

    if (ext == ".ktx2" || ext == ".ktx") {
        index = load_ktx2(path.generic_string(), type);
    } else if (ext == ".dds") {
        index = load_dds(path.generic_string(), type);
    } else {
        index = load_stb_image(path.generic_string(), type, generate_mipmaps);
    }

    // Cache the result
    (*cache)[path.generic_string()] = index;

    return index;
}

auto TextureManager::load_stb_image(const std::string& filepath,
                                   batleth::TextureType type,
                                   bool gen_mips) -> uint32_t {
    FED_INFO("Loading texture via stb_image: {}", filepath);

    // Load image data
    int width, height, channels;
    stbi_uc* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        FED_ERROR("Failed to load texture: {}", filepath);
        return 0;  // Return default texture index
    }

    uint32_t mip_levels = gen_mips ? batleth::calculate_mip_levels(width, height) : 1;

    FED_TRACE("Loaded {}x{} texture with {} channels, generating {} mip levels",
             width, height, channels, mip_levels);

    // Create image
    batleth::Image::Config img_config{};
    img_config.device = m_device.get_logical_device();
    img_config.allocator = m_allocator;
    img_config.width = static_cast<uint32_t>(width);
    img_config.height = static_cast<uint32_t>(height);
    img_config.mip_levels = mip_levels;
    img_config.format = VK_FORMAT_R8G8B8A8_SRGB;  // Use SRGB for albedo
    img_config.tiling = VK_IMAGE_TILING_OPTIMAL;
    img_config.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    img_config.aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
    img_config.create_view = true;

    auto image = std::make_unique<batleth::Image>(img_config);

    // Upload via staging buffer
    VkDeviceSize image_size = width * height * 4;

    batleth::Buffer staging_buffer(
        m_device,
        image_size,
        1,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    staging_buffer.map();
    staging_buffer.write_to_buffer(pixels);
    staging_buffer.unmap();

    // Free stb_image data
    stbi_image_free(pixels);

    // Copy to GPU
    VkCommandBuffer cmd = m_device.begin_single_time_commands();

    // Transition ALL mip levels to TRANSFER_DST (required for mipmap generation)
    image->transition_layout(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, mip_levels);

    // Copy buffer to image (mip 0)
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};

    ::vkCmdCopyBufferToImage(cmd, staging_buffer.get_buffer(), image->get_image(),
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Generate mipmaps if requested
    if (gen_mips && mip_levels > 1) {
        batleth::generate_mipmaps(m_device, cmd, image->get_image(), img_config.format,
                                 width, height, mip_levels);
    } else {
        // Just transition to SHADER_READ_ONLY
        image->transition_layout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    m_device.end_single_time_commands(cmd);

    // Create texture and add to appropriate array
    auto texture = std::make_unique<batleth::Texture>(std::move(image), type, filepath);

    uint32_t index = 0;
    switch (type) {
        case batleth::TextureType::Albedo:
            index = static_cast<uint32_t>(m_albedo_textures.size());
            m_albedo_textures.push_back(std::move(texture));
            break;
        case batleth::TextureType::Normal:
            index = static_cast<uint32_t>(m_normal_textures.size());
            m_normal_textures.push_back(std::move(texture));
            break;
        case batleth::TextureType::MetallicRoughness:
            index = static_cast<uint32_t>(m_pbr_textures.size());
            m_pbr_textures.push_back(std::move(texture));
            break;
        default:
            FED_ERROR("Unknown texture type");
            return 0;
    }

    m_descriptors_dirty = true;

    FED_INFO("Loaded texture: {} (index {})", filepath, index);
    return index;
}

auto TextureManager::load_ktx2(const std::string& filepath, batleth::TextureType type) -> uint32_t {
    FED_WARN("KTX2 loading not yet implemented: {}", filepath);
    return 0;  // Return default texture for now
}

auto TextureManager::load_dds(const std::string& filepath, batleth::TextureType type) -> uint32_t {
    FED_WARN("DDS loading not yet implemented: {}", filepath);
    return 0;  // Return default texture for now
}

auto TextureManager::upload_material(MaterialGPU& material) -> uint32_t {
    if (m_material_count >= m_max_materials) {
        FED_ERROR("Material buffer full (max: {})", m_max_materials);
        return 0;
    }

    uint32_t index = m_material_count++;
    m_material_data.push_back(material);

    // Upload to GPU via staging buffer
    batleth::Buffer staging_buffer(
        m_device,
        sizeof(MaterialGPU),
        1,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    staging_buffer.map();
    staging_buffer.write_to_buffer(&material);
    staging_buffer.unmap();

    VkCommandBuffer cmd = m_device.begin_single_time_commands();

    VkBufferCopy copy_region{};
    copy_region.srcOffset = 0;
    copy_region.dstOffset = index * sizeof(MaterialGPU);
    copy_region.size = sizeof(MaterialGPU);

    ::vkCmdCopyBuffer(cmd, staging_buffer.get_buffer(), m_material_buffer->get_buffer(), 1, &copy_region);

    m_device.end_single_time_commands(cmd);

    FED_TRACE("Uploaded material to index {}", index);
    return index;
}

auto TextureManager::update_material(uint32_t index, MaterialGPU& material) -> void {
    if (index >= m_material_count) {
        FED_ERROR("Invalid material index: {}", index);
        return;
    }

    m_material_data[index] = material;

    // Upload to GPU
    batleth::Buffer staging_buffer(
        m_device,
        sizeof(MaterialGPU),
        1,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    staging_buffer.map();
    staging_buffer.write_to_buffer(&material);
    staging_buffer.unmap();

    VkCommandBuffer cmd = m_device.begin_single_time_commands();

    VkBufferCopy copy_region{};
    copy_region.srcOffset = 0;
    copy_region.dstOffset = index * sizeof(MaterialGPU);
    copy_region.size = sizeof(MaterialGPU);

    ::vkCmdCopyBuffer(cmd, staging_buffer.get_buffer(), m_material_buffer->get_buffer(), 1, &copy_region);

    m_device.end_single_time_commands(cmd);

    FED_TRACE("Updated material at index {}", index);
}

auto TextureManager::upload_materials(std::vector<MaterialGPU>& materials) -> uint32_t {
    if (m_material_count + materials.size() > m_max_materials) {
        FED_ERROR("Not enough space in material buffer ({} + {} > {})",
                 m_material_count, materials.size(), m_max_materials);
        return 0;
    }

    uint32_t starting_index = m_material_count;
    m_material_count += static_cast<uint32_t>(materials.size());

    // Add to CPU-side copy
    m_material_data.insert(m_material_data.end(), materials.begin(), materials.end());

    // Upload to GPU via staging buffer (batch upload)
    VkDeviceSize upload_size = materials.size() * sizeof(MaterialGPU);

    batleth::Buffer staging_buffer(
        m_device,
        upload_size,
        1,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    staging_buffer.map();
    staging_buffer.write_to_buffer(materials.data());
    staging_buffer.unmap();

    VkCommandBuffer cmd = m_device.begin_single_time_commands();

    VkBufferCopy copy_region{};
    copy_region.srcOffset = 0;
    copy_region.dstOffset = starting_index * sizeof(MaterialGPU);
    copy_region.size = upload_size;

    ::vkCmdCopyBuffer(cmd, staging_buffer.get_buffer(), m_material_buffer->get_buffer(), 1, &copy_region);

    m_device.end_single_time_commands(cmd);

    FED_INFO("Batch uploaded {} materials starting at index {}", materials.size(), starting_index);
    return starting_index;
}

} // namespace klingon
