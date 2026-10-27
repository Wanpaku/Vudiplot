/*Copyright (C) 2026  Teg Miles

This file is part of Vudiplot library.

Vudiplot library is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License,
or any later version.

Vudiplot library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty
of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Vudiplot library. If not, see <https://www.gnu.org/licenses/>.*/

#include "ImGuiVulkanUtil.hpp"

#include <BufferUtils.hpp>
#include <ImGuiTextureMesh.hpp>
#include <lodepng.h>
#include <imgui_internal.h>

#include <chrono>
#include <format>
#include <filesystem>
#include <iostream>

ImGuiVulkanUtil::ImGuiVulkanUtil(
    const std::unique_ptr<GenPresentation>& gen_presentation,
    const std::unique_ptr<ShaderInitInfo>& shader_init_info,
    const uint32_t max_frames_in_flight, const std::string& texture_path)
    : gen_presentation(gen_presentation)
    , spv_shader_path(shader_init_info->spv_shader_path)
    , vert_func_name(shader_init_info->vert_func_name)
    , frag_func_name(shader_init_info->frag_func_name)
    , max_frames_in_flight(max_frames_in_flight)
    , texture_path(texture_path)

{
    create_semaphores_and_fences();
    const std::string cur_locale { setlocale(LC_CTYPE, "") };
    const auto dot_ind { cur_locale.find(".") };
    const std::string cur_lang { cur_locale.substr(0, dot_ind) };
    for (const auto& [_, value] : language_codes) {
        if (cur_lang == value) {
            cur_lang_code = cur_lang;
            setenv("LANGUAGE", cur_lang_code.c_str(), 1);

            break;
        }
    }
}

ImGuiVulkanUtil::~ImGuiVulkanUtil()
{
    clean_up();
}

void ImGuiVulkanUtil::create_vertex_buffers(const vk::DeviceSize buffer_size)
{
    const auto& memory_properties {
        gen_presentation->get_physical_device().getMemoryProperties()
    };
    const auto& logical_device { gen_presentation->get_logical_device() };

    auto buffer_utils { std::make_unique<BufferUtils>(
        memory_properties, logical_device,
        gen_presentation->get_queue_index()) };

    vertex_buffers.clear();
    vertex_buffers.reserve(max_frames_in_flight);
    vertex_buffer_memories.clear();
    vertex_buffer_memories.reserve(max_frames_in_flight);
    vertex_counts.assign(max_frames_in_flight, 0);

    for (std::size_t i = 0; i < max_frames_in_flight; ++i) {
        auto [vertex_buffer, vertex_buffer_memory]
            = buffer_utils->create_buffer(
                buffer_size, vk::BufferUsageFlagBits::eVertexBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible
                    | vk::MemoryPropertyFlagBits::eHostCoherent);
        vertex_buffers.emplace_back(std::move(vertex_buffer));
        vertex_buffer_memories.emplace_back(std::move(vertex_buffer_memory));
    }
}

void ImGuiVulkanUtil::create_index_buffers(const vk::DeviceSize buffer_size)
{
    const auto& memory_properties {
        gen_presentation->get_physical_device().getMemoryProperties()
    };
    const auto& logical_device { gen_presentation->get_logical_device() };

    auto buffer_utils { std::make_unique<BufferUtils>(
        memory_properties, logical_device,
        gen_presentation->get_queue_index()) };

    index_buffers.clear();
    index_buffers.reserve(max_frames_in_flight);
    index_buffer_memories.clear();
    index_buffer_memories.reserve(max_frames_in_flight);
    index_counts.assign(max_frames_in_flight, 0);

    for (std::size_t i = 0; i < max_frames_in_flight; ++i) {
        auto [index_buffer, index_buffer_memory] = buffer_utils->create_buffer(
            buffer_size, vk::BufferUsageFlagBits::eIndexBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible
                | vk::MemoryPropertyFlagBits::eHostCoherent);
        index_buffers.emplace_back(std::move(index_buffer));
        index_buffer_memories.emplace_back(std::move(index_buffer_memory));
    }
}

auto ImGuiVulkanUtil::init_ImGui(float width, float height,
                                 const std::string& font_path) -> bool
{
    // Initialize ImGui context
    IMGUI_CHECKVERSION();
    imgui_context = ImGui::CreateContext();
    if (!imgui_context) {
        std::cerr << gettext("Failed to create ImGui context!") << "\n";
        return false;
    }

    implot_context = ImPlot::CreateContext();
    if (!implot_context) {
        std::cerr << gettext("Failed to create ImPlot context!") << "\n";
        return false;
    }

    // Configure ImGui
    ImGuiIO& io = ImGui::GetIO();

    // Enable keyboard controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    // Inform ImGui that we support the new texture update protocol (v1.92+)
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    // Set display size
    io.DisplaySize = ImVec2(width, height);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

    // Download and set a font with a support of Cyrillic letters.
    ImFont* geist_nerd_mono = io.Fonts->AddFontFromFileTTF(
        font_path.c_str(), 25.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());

    if (geist_nerd_mono == nullptr) {
        std::cerr << gettext("Can't load font file: ") << font_path << "\n";
        return false;
    }

    io.FontDefault = geist_nerd_mono;

    ImGuiStyle style = ImGui::GetStyle();
    style.FontSizeBase = 25;
    set_up_vulkan_style();
    set_up_glassy_style();
    // Apply default style
    set_style(style_num);

    return true;
}

void ImGuiVulkanUtil::set_up_vulkan_style()
{
    vulkan_style = ImGui::GetStyle();
    vulkan_style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
    vulkan_style.Colors[ImGuiCol_TitleBgActive]
        = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    vulkan_style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    vulkan_style.Colors[ImGuiCol_Header] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    vulkan_style.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
    vulkan_style.WindowBorderSize = 0.0f;
    vulkan_style.WindowPadding = ImVec2(0, 0);
    vulkan_style.Colors[ImGuiCol_WindowBg] = vudiplot_colors.midnight_blue;
    vulkan_style.Colors[ImGuiCol_PopupBg] = vudiplot_colors.midnight_blue;
}

void ImGuiVulkanUtil::set_up_glassy_style()
{
    glassy_style = ImGui::GetStyle();
    glassy_style.WindowBorderSize = 0.0f;
    glassy_style.WindowPadding = ImVec2(0, 0);
    auto trans_black { vudiplot_colors.black };
    trans_black.w = 0.25;
    glassy_style.Colors[ImGuiCol_ModalWindowDimBg] = trans_black;
    glassy_style.Colors[ImGuiCol_Text] = vudiplot_colors.mild_sand;
    glassy_style.Colors[ImGuiCol_FrameBg] = vudiplot_colors.midnight_blue;
    glassy_style.Colors[ImGuiCol_PopupBg] = ImVec4(0, 0, 0, 0.7);
    glassy_style.Colors[ImGuiCol_WindowBg] = vudiplot_colors.forest_green;
}

void ImGuiVulkanUtil::set_style(GuiStyles gui_style)
{
    ImGuiStyle& style = ImGui::GetStyle();

    switch (gui_style) {
    case GuiStyles::Vulkan_style: {
        // Custom Vulkan style
        style = vulkan_style;
        break;
    }
    case GuiStyles::Classic: {
        // Classic style
        ImGui::StyleColorsClassic();
        break;
    }
    case GuiStyles::Dark: {
        // Dark style
        ImGui::StyleColorsDark();
        break;
    }
    case GuiStyles::Light: {
        // Light style
        ImGui::StyleColorsLight();
        break;
    }
    case GuiStyles::Glassy: {
        // Custom glassy style
        style = glassy_style;
        break;
    }
    default: {
        const std::string& message { gettext("Wrong style index: ")
                                     + std::to_string(
                                         static_cast<uint8_t>(gui_style)) };
        throw std::runtime_error(message);
    }
    }
}

auto ImGuiVulkanUtil::create_image(const vk::Extent2D& extent,
                                   const vk::Format format)
    -> std::pair<vk::raii::Image, vk::raii::DeviceMemory>
{
    const auto& memory_properties {
        gen_presentation->get_physical_device().getMemoryProperties()
    };
    const auto& logical_device { gen_presentation->get_logical_device() };

    auto buffer_utils { std::make_unique<BufferUtils>(
        memory_properties, logical_device,
        gen_presentation->get_queue_index()) };

    // Create optimized GPU image for texture storage
    // This image will be sampled by shaders during UI rendering
    auto [image, image_memory] = buffer_utils->create_image(
        extent.width, extent.height, format, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eSampled
            | vk::ImageUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal, 1,
        vk::SampleCountFlagBits::e1);

    return std::make_pair(std::move(image), std::move(image_memory));
}

auto ImGuiVulkanUtil::create_image_view(const vk::raii::Image& image,
                                        const vk::Format format)
    -> vk::raii::ImageView
{
    const auto& memory_properties {
        gen_presentation->get_physical_device().getMemoryProperties()
    };
    const auto& logical_device { gen_presentation->get_logical_device() };

    auto buffer_utils { std::make_unique<BufferUtils>(
        memory_properties, logical_device,
        gen_presentation->get_queue_index()) };

    // Create image view for shader access
    // The image view defines how shaders interpret the raw image data
    auto image_view = buffer_utils->create_imageview(
        image, format, vk::ImageAspectFlagBits::eColor, 1);

    return image_view;
}

auto ImGuiVulkanUtil::create_screenshot_buffer_and_memory()
    -> std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>
{
    const auto& memory_properties {
        gen_presentation->get_physical_device().getMemoryProperties()
    };
    const auto& logical_device { gen_presentation->get_logical_device() };

    auto buffer_utils { std::make_unique<BufferUtils>(
        memory_properties, logical_device,
        gen_presentation->get_queue_index()) };

    const vk::Extent2D extent = gen_presentation->get_swapchain_extent();

    const vk::DeviceSize buffer_size
        = vk::DeviceSize(extent.width) * vk::DeviceSize(extent.height) * 4;

    auto [staging_buffer, staging_buffer_memory] = buffer_utils->create_buffer(
        buffer_size, vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eHostVisible
            | vk::MemoryPropertyFlagBits::eHostCoherent);

    return std::make_pair(std::move(staging_buffer),
                          std::move(staging_buffer_memory));
}

void ImGuiVulkanUtil::create_staging_buffer(const vk::DeviceSize& upload_size,
                                            unsigned char* cur_data,
                                            const int tex_width,
                                            const int tex_height,
                                            vk::raii::Image& image)
{
    const auto& memory_properties {
        gen_presentation->get_physical_device().getMemoryProperties()
    };
    const auto& logical_device { gen_presentation->get_logical_device() };

    auto buffer_utils { std::make_unique<BufferUtils>(
        memory_properties, logical_device,
        gen_presentation->get_queue_index()) };

    // Create staging buffer for efficient CPU-to-GPU data transfer
    // Host-visible memory allows direct CPU access for data upload
    auto [staging_buffer, staging_buffer_memory] = buffer_utils->create_buffer(
        upload_size, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible
            | vk::MemoryPropertyFlagBits::eHostCoherent);

    // Map staging buffer memory and copy font data
    // Direct memory mapping provides the fastest path for data transfer
    void* data_staging = staging_buffer_memory.mapMemory(0, upload_size);
    // Map GPU memory to CPU address space
    memcpy(data_staging, cur_data, upload_size);
    // Copy font atlas data to GPU memory
    staging_buffer_memory.unmapMemory();
    // Unmap memory to ensure data consistency

    // Transition image to optimal layout for data reception
    // Vulkan requires explicit layout transitions for optimal performance and
    // correctness
    auto is_transition_ok { buffer_utils->transition_image_layout(
        image, vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal, 1,
        gen_presentation->get_presentation_queue()) };

    if (!is_transition_ok) {
        throw std::invalid_argument(gettext("Unsupported layout transition!"));
    }

    // Execute the actual buffer-to-image copy operation
    // This transfers  data from staging buffer to the final GPU image
    buffer_utils->copy_buffer_to_image(
        staging_buffer, image, tex_width, tex_height,
        gen_presentation->get_presentation_queue());

    // Transition image to shader-readable layout for rendering
    // Final layout optimization enables efficient sampling during UI rendering

    auto is_succes { buffer_utils->transition_image_layout(
        image, vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal, 1,
        gen_presentation->get_presentation_queue()) };

    if (!is_succes) {
        throw std::invalid_argument(gettext("Unsupported layout transition!"));
    }
}

auto ImGuiVulkanUtil::create_sampler() -> vk::raii::Sampler
{
    // Configure texture sampling parameters for optimal text rendering
    // These settings directly impact text quality and performance
    vk::SamplerCreateInfo sampler_info {
        .magFilter = vk::Filter::eLinear,
        // Smooth scaling when magnified
        .minFilter = vk::Filter::eLinear,
        // Smooth scaling when minified
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        // Smooth transitions between mip levels
        .addressModeU
        = vk::SamplerAddressMode::eClampToEdge, // Prevent texture wrapping
        .addressModeV
        = vk::SamplerAddressMode::eClampToEdge, // Clean edge handling
        .addressModeW = vk::SamplerAddressMode::eClampToEdge, // 3D consistency
        .borderColor
        = vk::BorderColor::eFloatOpaqueWhite // White border for clamped areas
    };
    // Create the GPU sampler object
    return vk::raii::Sampler(gen_presentation->get_logical_device(),
                             sampler_info);
}

auto ImGuiVulkanUtil::create_descriptor_pool(const uint32_t descr_count,
                                             const uint32_t max_sets)
    -> vk::raii::DescriptorPool
{
    assert(descr_count > 0);
    assert(max_sets > 0);
    vk::DescriptorPoolSize descr_pool_size {
        .type = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = descr_count
    };

    vk::DescriptorPoolCreateInfo descr_pool_info {
        .flags = vk::DescriptorPoolCreateFlagBits::
            eFreeDescriptorSet, // Allow individual descriptor set freeing
        .maxSets = max_sets, // Maximum number of descriptor sets
        .poolSizeCount = 1, // Number of pool size specifications
        .pPoolSizes = &descr_pool_size // size configuration
    };
    return vk::raii::DescriptorPool(gen_presentation->get_logical_device(),
                                    descr_pool_info);
}

auto ImGuiVulkanUtil::create_descriptor_set_layout()
    -> vk::raii::DescriptorSetLayout
{
    // Create descriptor set layout defining shader resource interface
    // This layout must match the binding declarations in the ImGui shaders
    vk::DescriptorSetLayoutBinding binding {
        .binding = 0, // Shader binding point 0
        .descriptorType
        = vk::DescriptorType::eCombinedImageSampler, //    Combined texture
                                                     //    and sampler
        .descriptorCount = 1, // Single texture binding

        // Single texture binding
        .stageFlags = vk::ShaderStageFlagBits::eFragment,

    };
    vk::DescriptorSetLayoutCreateInfo layout_info {
        .bindingCount = 1, // Number of bindings in layout
        .pBindings = &binding //    Binding configuration array
    };
    return vk::raii::DescriptorSetLayout(
        gen_presentation->get_logical_device(), layout_info);
}

auto ImGuiVulkanUtil::create_descriptor_set(
    const vk::raii::DescriptorSetLayout& descr_set_layout,
    const vk::raii::DescriptorPool& descr_pool) -> vk::raii::DescriptorSet
{
    // Allocate descriptor set from pool using the defined layout
    // This creates the actual binding that connects GPU resources to shaders

    std::vector<vk::DescriptorSetLayout> layouts(1, descr_set_layout);
    // Layout template array

    vk::DescriptorSetAllocateInfo alloc_info {
        .descriptorPool = descr_pool,
        // Source pool for allocation
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        // Number of sets to allocate
        .pSetLayouts = layouts.data()
    };

    return std::move(gen_presentation->get_logical_device()
                         .allocateDescriptorSets(alloc_info)
                         .front());
}

void ImGuiVulkanUtil::update_descriptor_set(
    const vk::raii::Sampler& sampler, const vk::raii::ImageView& image_view,
    vk::raii::DescriptorSet& descr_set)
{
    // Update descriptor set with actual texture and sampler resources
    // This final step connects the physical GPU resources to the shader
    // binding points

    vk::DescriptorImageInfo image_info {
        .sampler = *sampler,
        .imageView = *image_view,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };

    vk::WriteDescriptorSet write_set {
        .dstSet = *descr_set,
        // Target descriptor set
        .dstBinding = 0,
        // Binding point in shader
        .descriptorCount = 1,
        // Number of resources to bind
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        // Resource type
        .pImageInfo = &image_info,
        // Image resource information
    };
    gen_presentation->get_logical_device().updateDescriptorSets(write_set, {});
    // Execute the binding update
}

void ImGuiVulkanUtil::create_pipeline_layout()
{
    // Create pipeline cache
    vk::PipelineCacheCreateInfo pipeline_cache_info {};
    pipeline_cache = vk::raii::PipelineCache(
        gen_presentation->get_logical_device(), pipeline_cache_info);

    // Create pipeline layout
    vk::PushConstantRange push_constant_range {
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
        .offset = 0,
        .size = sizeof(PushConstBlock)
    };

    vk::PipelineLayoutCreateInfo pipeline_layout_info {
        .setLayoutCount = 1,
        .pSetLayouts = &*font_descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range,
    };

    pipeline_layout = vk::raii::PipelineLayout(
        gen_presentation->get_logical_device(), pipeline_layout_info);
}

auto ImGuiVulkanUtil::read_spv_file(const std::string& filename)
    -> std::vector<char>
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        const std::string error_message { gettext("Failed to open file: ")
                                          + filename };
        throw std::runtime_error(error_message);
    }
    std::vector<char> buffer(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
    return buffer;
}

auto ImGuiVulkanUtil::create_shader_module(
    const std::vector<char>& bytecode) const -> vk::raii::ShaderModule
{
    vk::ShaderModuleCreateInfo create_shader_module_info {
        .codeSize = bytecode.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t*>(bytecode.data())
    };
    vk::raii::ShaderModule shader_module {
        gen_presentation->get_logical_device(), create_shader_module_info
    };

    return shader_module;
}

void ImGuiVulkanUtil::create_graphics_pipeline()
{

    vk::raii::ShaderModule shader_module
        = create_shader_module(read_spv_file(spv_shader_path));

    vk::PipelineShaderStageCreateInfo vert_shader_stage_info {
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = shader_module,
        .pName = vert_func_name.c_str()
    };
    vk::PipelineShaderStageCreateInfo frag_shader_stage_info {
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = shader_module,
        .pName = frag_func_name.c_str()
    };

    std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages {
        vert_shader_stage_info, frag_shader_stage_info
    };

    // Vertex input
    vk::VertexInputBindingDescription binding_description {
        .binding = 0,
        .stride = sizeof(ImDrawVert),
        .inputRate = vk::VertexInputRate::eVertex
    };

    std::array<vk::VertexInputAttributeDescription, 3> attribute_descriptions {
        vk::VertexInputAttributeDescription {
                                             .location = 0,
                                             .binding = 0,
                                             .format = vk::Format::eR32G32Sfloat,
                                             .offset = offsetof(ImDrawVert, pos) },

        vk::VertexInputAttributeDescription {
                                             .location = 1,
                                             .binding = 0,
                                             .format = vk::Format::eR32G32Sfloat,
                                             .offset = offsetof(ImDrawVert, uv)  },

        vk::VertexInputAttributeDescription {
                                             .location = 2,
                                             .binding = 0,
                                             .format = vk::Format::eR8G8B8A8Unorm,
                                             .offset = offsetof(ImDrawVert, col) }
    };

    vk::PipelineVertexInputStateCreateInfo vertex_input_info {
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_description,
        .vertexAttributeDescriptionCount
        = static_cast<uint32_t>(attribute_descriptions.size()),
        .pVertexAttributeDescriptions = attribute_descriptions.data()
    };

    // Input assembly
    vk::PipelineInputAssemblyStateCreateInfo input_assembly {
        .topology = vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable = VK_FALSE
    };

    // Viewport and scissor
    vk::PipelineViewportStateCreateInfo viewport_state {
        .viewportCount = 1,
        .pViewports = nullptr, // Dynamic state
        .scissorCount = 1,
        .pScissors = nullptr // Dynamic state
    };

    // Rasterization
    vk::PipelineRasterizationStateCreateInfo rasterizer {
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };

    // Multisampling
    vk::PipelineMultisampleStateCreateInfo multisampling {
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = VK_FALSE,
    };

    // Depth and stencil testing
    vk::PipelineDepthStencilStateCreateInfo depth_stencil {
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_FALSE,
        .depthCompareOp = vk::CompareOp::eLessOrEqual,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
    };

    // Color blending
    vk::PipelineColorBlendAttachmentState color_blend_attachment {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
        .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
        .colorBlendOp = vk::BlendOp::eAdd,

        .srcAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
        .dstAlphaBlendFactor = vk::BlendFactor::eZero,
        .alphaBlendOp = vk::BlendOp::eAdd,
        .colorWriteMask = vk::ColorComponentFlagBits::eR
            | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB
            | vk::ColorComponentFlagBits::eA,
    };

    vk::PipelineColorBlendStateCreateInfo color_blending {
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment
    };

    // Dynamic state
    std::vector<vk::DynamicState> dynamic_states
        = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

    vk::PipelineDynamicStateCreateInfo dynamic_state {
        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data()
    };

    // Create the graphics pipeline with dynamic rendering

    vk::StructureChain<vk::GraphicsPipelineCreateInfo,
                       vk::PipelineRenderingCreateInfo>
        pipeline_create_info_chain = {
            { .stageCount = static_cast<uint32_t>(shader_stages.size()),
             .pStages = shader_stages.data(),
             .pVertexInputState = &vertex_input_info,
             .pInputAssemblyState = &input_assembly,
             .pViewportState = &viewport_state,
             .pRasterizationState = &rasterizer,
             .pMultisampleState = &multisampling,
             .pDepthStencilState = &depth_stencil,
             .pColorBlendState = &color_blending,
             .pDynamicState = &dynamic_state,
             .layout = pipeline_layout,
             .renderPass = nullptr,
             .basePipelineHandle = nullptr },
            { .colorAttachmentCount = 1,
             .pColorAttachmentFormats
              = &gen_presentation->get_swapchain_surface_format().format,
             .depthAttachmentFormat = gen_presentation->get_depth_format() }
    };

    pipeline = vk::raii::Pipeline(
        gen_presentation->get_logical_device(), pipeline_cache,
        pipeline_create_info_chain.get<vk::GraphicsPipelineCreateInfo>());
}

auto ImGuiVulkanUtil::load_textures() -> bool
{
    // Loading textures from images in .ktx2 format and register them
    // in texture manager
    std::vector<std::filesystem::path> image_paths;
    try {
        if (std::filesystem::exists(texture_path)
            && std::filesystem::is_directory(texture_path)) {
            for (const auto& file_path :
                 std::filesystem::directory_iterator(texture_path)) {
                if (file_path.is_regular_file()
                    && file_path.path().extension() == ".ktx2") {
                    image_paths.emplace_back(file_path.path());
                }
            }
        }
    } catch (std::filesystem::filesystem_error const& error) {
        std::cerr << error.what() << '\n';
        return false;
    }

    size_t available_fonts_count = ImGui::GetIO().Fonts->Fonts.size();

    auto images_amount { image_paths.size() };

    total_draw_pool_size += images_amount + available_fonts_count;

    draw_tex_descriptor_pool
        = create_descriptor_pool(total_draw_pool_size, total_draw_pool_size);

    if (images_amount > 0) {
        loaded_textures_descriptor_pool
            = create_descriptor_pool(images_amount, images_amount);

        texture_manager = std::make_unique<ImGuiTextureManager>(
            gen_presentation->get_logical_device(),
            loaded_textures_descriptor_pool);

        for (const auto& path : image_paths) {

            auto mesh_info {
                std::make_unique<MeshInitInfoFile>(MeshInitInfoFile {
                    .texture_path = path.string(),
                    .physical_device = gen_presentation->get_physical_device(),
                    .logical_device = gen_presentation->get_logical_device(),
                    .queue_index = gen_presentation->get_queue_index(),
                    .graphics_queue
                    = gen_presentation->get_presentation_queue() })

            };

            auto texture_mesh { std::make_unique<ImGuiTextureMesh>(
                mesh_info) };

            const std::string& tex_name { path.stem().string() };
            texture_manager->register_texture(
                texture_mesh->get_texture_image(),
                texture_mesh->get_texture_image_memory(),
                texture_mesh->get_texture_image_view(),
                texture_mesh->get_texture_sampler(),
                texture_mesh->get_image_size().image_width,
                texture_mesh->get_image_size().image_height, tex_name);
        }
    }
    return true;
}

auto ImGuiVulkanUtil::init_resources() -> bool
{
    // Extract font atlas data from ImGui's internal font system
    // ImGui generates a texture atlas containing all glyphs needed for text
    // rendering

    ImGuiIO& io = ImGui::GetIO();
    unsigned char* font_data;
    // Raw pixel data from font atlas
    int tex_width, tex_height;
    // Dimensions of the generated font atlas

    io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);
    // Calculate total memory requirements for GPU transfer
    // Each pixel contains 4 bytes (RGBA) requiring precise memory allocation
    vk::DeviceSize upload_size = tex_width * tex_height * 4 * sizeof(char);

    // Define image dimensions and create extent structure
    // Vulkan requires explicit specification of all image dimensions
    vk::Extent2D font_extent {
        static_cast<uint32_t>(tex_width),
        // Image width in pixels
        static_cast<uint32_t>(tex_height),
        // Image height in pixels
    };

    if (!load_textures()) {
        std::cerr << gettext("Textures doesn't loaded!") << "\n";
        return false;
    }

    create_vertex_buffers(1024 * sizeof(ImDrawVert));
    if (vertex_buffers.empty() || vertex_buffer_memories.empty()) {
        std::cerr << gettext("Vertex buffers are empty!") << "\n";
        return false;
    }
    create_index_buffers(1024 * sizeof(ImDrawVert));
    if (index_buffers.empty() || index_buffer_memories.empty()) {
        std::cerr << gettext("Index buffers are empty!") << "\n";
        return false;
    }

    std::tie(screenshot_buffer, screenshot_buffer_memory)
        = create_screenshot_buffer_and_memory();
    if (screenshot_buffer == nullptr || screenshot_buffer_memory == nullptr) {
        std::cout << gettext("Screenshot buffer is nullptr!") << "\n";
        return false;
    }

    std::tie(font_image, font_memory)
        = create_image(font_extent, vk::Format::eR8G8B8A8Unorm);
    if (font_image == nullptr || font_memory == nullptr) {
        std::cerr << gettext("Font image is nullptr") << "\n";
        return false;
    }

    font_image_view
        = create_image_view(font_image, vk::Format::eR8G8B8A8Unorm);
    if (font_image_view == nullptr) {
        std::cerr << gettext("Font image view is nullptr!") << "\n";
        return false;
    }

    create_staging_buffer(upload_size, font_data, tex_width, tex_height,
                          font_image);

    font_sampler = create_sampler();
    if (font_sampler == nullptr) {
        std::cerr << gettext("Font sampler is nullptr!") << "\n";
        return false;
    }

    font_descriptor_pool = create_descriptor_pool(1, 2);
    if (font_descriptor_pool == nullptr) {
        std::cerr << gettext("Font descriptor pool is nullptr!") << "\n";
        return false;
    }

    font_descriptor_set_layout = create_descriptor_set_layout();
    if (font_descriptor_set_layout == nullptr) {
        std::cerr << gettext("Font descriptor set layout is nullptr!") << "\n";
        return false;
    }

    font_descriptor_set = create_descriptor_set(font_descriptor_set_layout,
                                                font_descriptor_pool);
    if (font_descriptor_set == nullptr) {
        std::cerr << gettext("Font descriptor set is nullptr!") << "\n";
        return false;
    }

    update_descriptor_set(font_sampler, font_image_view, font_descriptor_set);

    create_pipeline_layout();
    if (pipeline_layout == nullptr) {
        std::cerr << gettext("Pipeline layout is nullptr!") << "\n";
        return false;
    }

    create_graphics_pipeline();
    if (pipeline == nullptr) {
        std::cerr << gettext("Graphics pipeline is nullptr") << "\n";
        return false;
    }

    return true;
}

auto ImGuiVulkanUtil::menu_item_with_icon(ImTextureID icon,
                                          const ImVec2& icon_size,
                                          const char* label,
                                          const char* shortcut, bool enabled)
    -> bool
{
    // Helper function for creating ImGui::MenuItem with icon
    ImGuiWindow* window = ImGui::GetCurrentWindow();

    if (window->SkipItems) {
        return false;
    }

    const ImGuiStyle& style = imgui_context->Style;

    // Match MenuItem() text measurements.
    const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
    const ImVec2 shortcut_size = shortcut
        ? ImGui::CalcTextSize(shortcut, nullptr)
        : ImVec2(0.0f, 0.0f);

    // Menu item height.
    const float height
        = ImMax(icon_size.y, label_size.y) + style.FramePadding.y * 2.0f;

    // Width of the item.
    float width = style.WindowPadding.x + icon_size.x
        + style.ItemInnerSpacing.x + label_size.x;

    if (shortcut) {
        width += style.ItemInnerSpacing.x + shortcut_size.x;
    }

    width += style.WindowPadding.x;

    const ImVec2 pos = ImGui::GetCursorScreenPos();

    // Make the entire menu item clickable.
    ImGui::PushID(label);

    if (!enabled) {
        ImGui::BeginDisabled();
    }

    const bool pressed
        = ImGui::InvisibleButton("##MenuItem", ImVec2(width, height));

    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();

    // Menu background.
    if (hovered) {
        const ImGuiCol col
            = active ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered;

        window->DrawList->AddRectFilled(pos,
                                        ImVec2(pos.x + width, pos.y + height),
                                        ImGui::GetColorU32(col));
    }

    // Icon.
    if (icon) {
        const float iconX = pos.x + style.WindowPadding.x;
        const float iconY = pos.y + (height - icon_size.y) * 0.5f;

        window->DrawList->AddImage(
            icon, ImVec2(iconX, iconY),
            ImVec2(iconX + icon_size.x, iconY + icon_size.y));
    }

    // Label.
    const float textX = pos.x + style.WindowPadding.x + icon_size.x
        + style.ItemInnerSpacing.x;

    const float textY = pos.y + (height - label_size.y) * 0.5f;

    window->DrawList->AddText(
        ImVec2(textX, textY),
        ImGui::GetColorU32(enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled),
        label);

    // Shortcut.
    if (shortcut) {
        const float shortcutX
            = pos.x + width - style.WindowPadding.x - shortcut_size.x;

        const float shortcutY = pos.y + (height - shortcut_size.y) * 0.5f;

        window->DrawList->AddText(
            ImVec2(shortcutX, shortcutY),
            ImGui::GetColorU32(enabled ? ImGuiCol_Text
                                       : ImGuiCol_TextDisabled),
            shortcut);
    }

    if (!enabled)
        ImGui::EndDisabled();

    ImGui::PopID();

    return pressed && enabled;
}

void ImGuiVulkanUtil::add_background_to_menu(const ImTextureID& image)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 position = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();

    draw_list->AddImage(image, position,
                        ImVec2(position.x + size.x, position.y + size.y));
}

void ImGuiVulkanUtil::draw_demos()
{
    // Draw selected demo guides
    set_style(style_num);
    if (imgui_demo_state.show_imgui_demo) {
        ImGui::ShowDemoWindow();
    }

    if (imgui_demo_state.show_imgui_user_guide) {
        ImGui::ShowUserGuide();
    }

    if (imgui_demo_state.show_imgui_style_editor) {
        ImGui::ShowStyleEditor();
    }

    if (imgui_demo_state.show_imgui_about_window) {
        ImGui::ShowAboutWindow();
    }

    if (imgui_demo_state.show_imgui_metrics_window) {
        ImGui::ShowMetricsWindow();
    }

    if (implot_demo_state.show_implot_demo) {
        ImPlot::ShowDemoWindow();
    }

    if (implot_demo_state.show_implot_user_guide) {
        ImPlot::ShowUserGuide();
    }

    if (implot_demo_state.show_implot_style_editor) {
        ImPlot::ShowStyleEditor();
    }

    if (implot_demo_state.show_implot_metrics_window) {
        ImPlot::ShowMetricsWindow();
    }

    if (implot_demo_state.show_implot_colormap_selector) {
        ImPlot::ShowColormapSelector(gettext("Color map selector"));
    }
}

void ImGuiVulkanUtil::center_widget_width_position(const float widget_width)
{
    // Center widget horizontally
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()
                         + (ImGui::GetContentRegionAvail().x - widget_width)
                             * 0.5f);
}

void ImGuiVulkanUtil::center_widget_height_position(const float widget_height)
{
    // Center widget vertically
    ImGui::SetCursorPosY(ImGui::GetCursorPosY()
                         + (ImGui::GetContentRegionAvail().y - widget_height));
}

auto ImGuiVulkanUtil::show_demos(bool& is_running) -> bool
{
    const auto& blue_silk_bg { texture_manager->get_texture("blue_silk") };
    if (blue_silk_bg == ImTextureID_Invalid) {
        std::cerr << gettext("Blue silk background is invalid!") << "\n";
        return false;
    }

    const auto& red_and_blue_bg { texture_manager->get_texture(
        "red_and_blue") };
    if (red_and_blue_bg == ImTextureID_Invalid) {
        std::cerr << gettext("Red and blue background is invalid!") << "\n";
        return false;
    }

    const auto& exit_image { texture_manager->get_texture("exit") };
    if (exit_image == ImTextureID_Invalid) {
        std::cerr << gettext("Exit image is invalid!") << "\n";
        return false;
    }

    if (ImGui::BeginMainMenuBar()) {
        if (style_num == GuiStyles::Glassy) {
            add_background_to_menu(blue_silk_bg);
        }

        if (ImGui::BeginMenu(gettext("File"))) {

            if (style_num == GuiStyles::Glassy) {
                add_background_to_menu(red_and_blue_bg);
            }

            const auto& save_image { texture_manager->get_texture("save") };
            if (save_image == ImTextureID_Invalid) {
                std::cerr << gettext("Save image is invalid!") << "\n";
                return false;
            }
            if (menu_item_with_icon(texture_manager->get_texture("save"),
                                    menubar_icon_size,
                                    gettext("Save screenshot"), "Ctr+S")) {
                save_screenshot_dialog();
                ImGui::CloseCurrentPopup();
            }

            ImGui::Separator();
            if (menu_item_with_icon(exit_image, menubar_icon_size,
                                    gettext("Exit"), "Ctrl+Q")) {
                is_running = false;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(gettext("Settings"))) {

            if (style_num == GuiStyles::Glassy) {
                add_background_to_menu(red_and_blue_bg);
            }

            if (ImGui::BeginMenu("Imgui:")) {

                if (style_num == GuiStyles::Glassy) {
                    add_background_to_menu(red_and_blue_bg);
                }

                ImGui::MenuItem("ImGui demo window", nullptr,
                                &imgui_demo_state.show_imgui_demo);
                ImGui::MenuItem("ImGui user guide", nullptr,
                                &imgui_demo_state.show_imgui_user_guide);
                ImGui::MenuItem("ImGui style editor", nullptr,
                                &imgui_demo_state.show_imgui_style_editor);
                ImGui::MenuItem("ImGui metrics window", nullptr,
                                &imgui_demo_state.show_imgui_metrics_window);

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("ImPlot:")) {

                if (style_num == GuiStyles::Glassy) {
                    add_background_to_menu(red_and_blue_bg);
                }

                ImGui::MenuItem("ImPlot demo window", nullptr,
                                &implot_demo_state.show_implot_demo);
                ImGui::MenuItem("ImPlot user guide", nullptr,
                                &implot_demo_state.show_implot_user_guide);
                ImGui::MenuItem("ImPlot style editor", nullptr,
                                &implot_demo_state.show_implot_style_editor);
                ImGui::MenuItem("ImPlot metrics window", nullptr,
                                &implot_demo_state.show_implot_metrics_window);
                ImGui::MenuItem(
                    "ImPlot colormap selector", nullptr,
                    &implot_demo_state.show_implot_colormap_selector);

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu(gettext("Themes"))) {

                if (style_num == GuiStyles::Glassy) {
                    add_background_to_menu(red_and_blue_bg);
                }

                if (ImGui::RadioButton(
                        "Vulkan style", &theme_button,
                        static_cast<int>(GuiStyles::Vulkan_style))) {
                    style_num = GuiStyles::Vulkan_style;
                    set_style(style_num);
                }
                if (ImGui::RadioButton("Dark", &theme_button,
                                       static_cast<int>(GuiStyles::Dark))) {
                    style_num = GuiStyles::Dark;
                    set_style(style_num);
                }
                if (ImGui::RadioButton("Classic", &theme_button,
                                       static_cast<int>(GuiStyles::Classic))) {
                    style_num = GuiStyles::Classic;
                    set_style(style_num);
                }
                if (ImGui::RadioButton("Light", &theme_button,
                                       static_cast<int>(GuiStyles::Light))) {
                    style_num = GuiStyles::Light;
                    set_style(style_num);
                }

                if (ImGui::RadioButton("Glassy", &theme_button,
                                       static_cast<int>(GuiStyles::Glassy))) {
                    style_num = GuiStyles::Glassy;
                    set_style(style_num);
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(gettext("Help"))) {

            if (style_num == GuiStyles::Glassy) {

                add_background_to_menu(red_and_blue_bg);
            }
            const auto& about_image { texture_manager->get_texture("about") };
            if (about_image == ImTextureID_Invalid) {
                std::cerr << gettext("About image is invalid!") << "\n";
                return false;
            }
            if (menu_item_with_icon(texture_manager->get_texture("about"),
                                    menubar_icon_size, gettext("About ImGui"),
                                    "F1")) {
                imgui_demo_state.show_imgui_about_window
                    = imgui_demo_state.show_imgui_about_window == false
                    ? true
                    : false;
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

    const auto& dark_green_leaves_bg { texture_manager->get_texture(
        "dark_green_leaves") };
    if (dark_green_leaves_bg == ImTextureID_Invalid) {
        std::cerr << gettext("Dark green leaves background is invalid!")
                  << "\n";
        return false;
    }

    const auto& vulkan_logo { texture_manager->get_texture("vulkan_logo") };
    if (vulkan_logo == ImTextureID_Invalid) {
        std::cerr << gettext("Vulkan logo background is invalid!") << "\n";
        return false;
    }

    if (ImGui::Begin("Main area", nullptr, window_flags)) {
        ImVec2 image_size = ImGui::GetContentRegionAvail();

        if (style_num == GuiStyles::Glassy) {
            ImGui::Image(dark_green_leaves_bg, image_size);
        }

        if (style_num == GuiStyles::Vulkan_style) {
            ImGui::Image(vulkan_logo, image_size);
        }
        ImVec2 main_area_pos { ImGui::GetWindowPos().x,
                               ImGui::GetWindowPos().y };

        ImGui::SetCursorScreenPos(main_area_pos);

        std::vector<std::string> avail_languages;
        avail_languages.reserve(language_codes.size());
        for (const auto& code : language_codes) {
            avail_languages.emplace_back(code.first);
        }
        const std::string& lang_combo_name { gettext("Interface language") };

        float combo_width
            = ImGui::CalcTextSize(
                  (lang_combo_name + avail_languages[selected_language])
                      .c_str())
                  .x;

        center_widget_width_position(combo_width);

        ImGuiComboFlags combo_flags { ImGuiComboFlags_WidthFitPreview };
        const char* combo_preview_value
            = avail_languages[selected_language].data();
        if (ImGui::BeginCombo(lang_combo_name.c_str(), combo_preview_value,
                              combo_flags)) {
            for (std::size_t n = 0; n < avail_languages.size(); n++) {
                const bool is_selected = (selected_language == n);
                if (ImGui::Selectable(avail_languages[n].data(),
                                      is_selected)) {
                    selected_language = n;
                    std::string cur_lang {
                        language_codes[avail_languages[n]]
                    };
                    setenv("LANGUAGE", cur_lang.c_str(), 1);
                    cur_lang += ".utf8";
                    setlocale(LC_ALL, cur_lang.c_str());
                }

                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        center_widget_width_position(menubar_icon_size.x);

        const auto& user_manual_image { texture_manager->get_texture(
            "user_manual") };
        if (user_manual_image == ImTextureID_Invalid) {
            std::cerr << gettext("User manual image is invalid!") << "\n";
            return false;
        }

        if (ImGui::ImageButton("ImGui demo window", user_manual_image,
                               menubar_icon_size)) {
            imgui_demo_state.show_imgui_demo
                = imgui_demo_state.show_imgui_demo == true ? false : true;
        }

        ImGui::SameLine();

        if (ImGui::ImageButton("Exit button", exit_image, menubar_icon_size)) {
            is_running = false;
        }

        const std::string& message { gettext(
            "For changing themes press Alt+S") };
        float message_with = ImGui::CalcTextSize(message.c_str()).x;
        center_widget_width_position(message_with);
        center_widget_height_position(30.f);

        ImGui::PushStyleColor(ImGuiCol_Text, vudiplot_colors.mild_sand);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size = ImGui::CalcTextSize(message.c_str());

        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::ColorConvertFloat4ToU32(vudiplot_colors.midnight_blue));
        ImGui::TextUnformatted(message.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::End();

    draw_demos();

    return true;
}

auto ImGuiVulkanUtil::create_menubar(bool& is_running) -> bool
{
    const auto& blue_silk_bg { texture_manager->get_texture("blue_silk") };
    if (blue_silk_bg == ImTextureID_Invalid) {
        std::cerr << gettext("Blue silk background is invalid!") << "\n";
        return false;
    }

    const auto& red_and_blue_bg { texture_manager->get_texture(
        "red_and_blue") };
    if (red_and_blue_bg == ImTextureID_Invalid) {
        std::cerr << gettext("Red and blue background is invalid!") << "\n";
        return false;
    }

    if (ImGui::BeginMainMenuBar()) {

        if (style_num == GuiStyles::Glassy) {
            add_background_to_menu(blue_silk_bg);
        }

        if (ImGui::BeginMenu(gettext("File"))) {
            if (style_num == GuiStyles::Glassy) {
                add_background_to_menu(red_and_blue_bg);
            }

            const auto& save_image { texture_manager->get_texture("save") };
            if (save_image == ImTextureID_Invalid) {
                std::cerr << gettext("Save image is invalid!") << "\n";
                return false;
            }
            if (menu_item_with_icon(texture_manager->get_texture("save"),
                                    menubar_icon_size,
                                    gettext("Save screenshot"), "Ctr+S")) {
                save_screenshot_dialog();
                ImGui::CloseCurrentPopup();
            }

            ImGui::Separator();
            const auto& exit_image { texture_manager->get_texture("exit") };
            if (exit_image == ImTextureID_Invalid) {
                std::cerr << gettext("Exit image is invalid!") << "\n";
                return false;
            }

            if (menu_item_with_icon(exit_image, menubar_icon_size,
                                    gettext("Exit"), "Ctr+Q")) {
                is_running = false;
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(gettext("Settings"))) {

            if (style_num == GuiStyles::Glassy) {

                add_background_to_menu(red_and_blue_bg);
            }

            const auto& home_image { texture_manager->get_texture("home") };
            if (home_image == ImTextureID_Invalid) {
                std::cerr << gettext("Home image is invalid!") << "\n";
                return false;
            }
            if (menu_item_with_icon(home_image, menubar_icon_size,
                                    gettext("Restore zoom"), "Alt+Z ")) {
                ImPlot::SetNextAxesToFit();
            }

            if (ImGui::BeginMenu(gettext("Interface language"))) {

                if (style_num == GuiStyles::Glassy) {
                    add_background_to_menu(red_and_blue_bg);
                }

                for (const auto& [key, value] : language_codes) {
                    if (ImGui::RadioButton(key.c_str(),
                                           cur_lang_code == value)) {
                        cur_lang_code = value;
                        setenv("LANGUAGE", value.c_str(), 1);
                        const std::string cur_lang { value + ".utf8" };
                        setlocale(LC_ALL, cur_lang.c_str());
                    }
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu(gettext("Themes Alt+S"))) {

                if (style_num == GuiStyles::Glassy) {
                    add_background_to_menu(red_and_blue_bg);
                }

                if (ImGui::RadioButton(
                        "Vulkan style", &theme_button,
                        static_cast<int>(GuiStyles::Vulkan_style))) {
                    style_num = GuiStyles::Vulkan_style;
                    set_style(style_num);
                }
                if (ImGui::RadioButton("Dark", &theme_button,
                                       static_cast<int>(GuiStyles::Dark))) {
                    style_num = GuiStyles::Dark;
                    set_style(style_num);
                }
                if (ImGui::RadioButton("Classic", &theme_button,
                                       static_cast<int>(GuiStyles::Classic))) {
                    style_num = GuiStyles::Classic;
                    set_style(style_num);
                }
                if (ImGui::RadioButton("Light", &theme_button,
                                       static_cast<int>(GuiStyles::Light))) {
                    style_num = GuiStyles::Light;
                    set_style(style_num);
                }

                if (ImGui::RadioButton("Glassy", &theme_button,
                                       static_cast<int>(GuiStyles::Glassy))) {
                    style_num = GuiStyles::Glassy;
                    set_style(style_num);
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu(gettext("ImPlot colormap selector:"))) {

                if (style_num == GuiStyles::Glassy) {
                    add_background_to_menu(red_and_blue_bg);
                }

                ImGui::MenuItem(
                    gettext("ImPlot colormap selector"), nullptr,
                    &implot_demo_state.show_implot_colormap_selector);

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    return true;
}

template <std::derived_from<VudiInit::PlotInit> T>
auto ImGuiVulkanUtil::plot_frame(const std::unique_ptr<T>& plot_param,
                                 bool& is_running) -> bool

{

    if (!plot_param) {
        std::cerr << gettext("The plot param is nullptr.") << "\n";
        return false;
    }
    const std::string& screenshot_saved_window { gettext(
        "The screenshot is saved") };
    ImGui::NewFrame();
    if (screenshot_is_saved) {
        ImGui::OpenPopup(screenshot_saved_window.c_str());
        screenshot_is_saved = false;
    }
    save_screenshot_success_window(screenshot_saved_window);

    if constexpr (!std::is_same_v<T, VudiInit::DemoPlot>) {

        if (!create_menubar(is_running)) {
            std::cerr << gettext("Creating plot menubar failed!") << "\n";
            return false;
        }

        // Match plot to free window size
        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(
            ImVec2(viewport->WorkPos.x, viewport->WorkPos.y));
        ImGui::SetNextWindowSize(
            ImVec2(viewport->WorkSize.x, viewport->WorkSize.y));

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("Main_plot_area", nullptr, window_flags)) {
            if (implot_demo_state.show_implot_colormap_selector) {
                if (ImGui::BeginCombo(gettext("ImPlot colormap"),
                                      ImPlot::GetColormapName(
                                          plot_param->implot_colormap))) {
                    for (int i = 0; i < ImPlot::GetColormapCount(); ++i) {
                        const bool selected
                            = (i == plot_param->implot_colormap);

                        if (ImGui::Selectable(ImPlot::GetColormapName(i),
                                              selected)) {
                            plot_param->implot_colormap
                                = static_cast<ImPlotColormap>(i);
                            ImPlot::BustColorCache();
                        }

                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    ImGui::EndCombo();
                }
            }
            ImPlot::PushColormap(plot_param->implot_colormap);

            if constexpr (std::is_same_v<T, VudiInit::LinePlot>) {
                if (!line_plot(plot_param)) {
                    std::cerr << gettext("Plot line failed!") << "\n";
                    ImGui::End();
                    ImGui::Render();
                    return false;
                }
            } else if constexpr (std::is_same_v<T, VudiInit::HeatmapsPlot>) {
                if (!heatmaps_plot(plot_param)) {
                    std::cerr << gettext("Heatmaps failed!") << "\n";
                    ImGui::End();
                    ImGui::Render();
                    return false;
                }

            } else if constexpr (std::is_same_v<T, VudiInit::ImagePlot>) {
                if (!image_plot(plot_param)) {
                    std::cerr << gettext("Image plot failed!") << "\n";
                    ImGui::End();
                    ImGui::Render();
                    return false;
                }
            } else {
                std::cerr << gettext(
                    "Wrong type of the plot parameters pointer.")
                          << "\n";
                ImGui::End();
                ImGui::Render();
                return false;
            }

            if (plot_param->implot_colormap >= 0
                && plot_param->implot_colormap < ImPlot::GetColormapCount()) {
                ImPlot::PopColormap();
            }
        }
        ImGui::End();

    } else {
        if (!show_demos(is_running)) {
            std::cerr << gettext("Show demos failed!") << "\n";
            return false;
        }
    }

    ImGui::Render();
    return true;
}

template auto ImGuiVulkanUtil::plot_frame(
    const std::unique_ptr<VudiInit::LinePlot>& plot_params, bool& is_running)
    -> bool;

template auto ImGuiVulkanUtil::plot_frame(
    const std::unique_ptr<VudiInit::HeatmapsPlot>& plot_params,
    bool& is_running) -> bool;

template auto ImGuiVulkanUtil::plot_frame(
    const std::unique_ptr<VudiInit::DemoPlot>& plot_params, bool& is_running)
    -> bool;

template auto ImGuiVulkanUtil::plot_frame(
    const std::unique_ptr<VudiInit::ImagePlot>& plot_params, bool& is_running)
    -> bool;

auto ImGuiVulkanUtil::line_plot(
    const std::unique_ptr<VudiInit::LinePlot>& plot_param) -> bool
{
    ImVec2 plot_size = ImGui::GetContentRegionAvail();
    ImPlotFlags implot_flags = ImPlotFlags_None;

    if (plot_param->legends.empty()
        || (plot_param->legends.size() < plot_param->x_axis.size())) {
        implot_flags = ImPlotFlags_NoLegend;
    }

    if (ImPlot::BeginPlot(plot_param->main_plot_name.c_str(), plot_size,
                          implot_flags)) {

        ImPlot::SetupAxis(ImAxis_X1, plot_param->x_axis_name.c_str());
        ImPlot::SetupAxis(ImAxis_Y1, plot_param->y_axis_name.c_str());

        if (plot_param->x_axis.size() != plot_param->y_axis.size()) {
            std::cerr << (gettext("Sizes of x_axis and y_axis "
                                  "in the plot don't match!"))
                      << "\n";
            ImPlot::EndPlot();
            return false;
        }

        for (std::size_t i { 0 }; i < plot_param->x_axis.size(); ++i) {
            if (plot_param->x_axis[i].size() != plot_param->y_axis[i].size()) {
                const std::string message {
                    gettext("Sizes of axes of the plot number ")
                    + std::to_string(i) + gettext(" don't match!")
                };
                std::cerr << message << "\n";
                ImPlot::EndPlot();
                return false;
            }
            const std::string& cur_plot_name
                = implot_flags & ImPlotFlags_NoLegend ? std::to_string(i)
                                                      : plot_param->legends[i];

            ImPlotSpec spec = i < plot_param->implot_specs.size()
                ? plot_param->implot_specs[i]
                : ImPlotSpec();

            ImPlot::PlotLine(cur_plot_name.c_str(),
                             plot_param->x_axis[i].data(),
                             plot_param->y_axis[i].data(),
                             plot_param->x_axis[i].size(), spec);
        }

        ImPlot::EndPlot();
    } else {
        std::cerr << gettext("BeginPlot in Line plot is failed!") << "\n";
        return false;
    }

    return true;
}

auto ImGuiVulkanUtil::heatmaps_plot(
    const std::unique_ptr<VudiInit::HeatmapsPlot>& plot_param) -> bool
{
    auto total_items
        = static_cast<double>(plot_param->rows * plot_param->cols);

    if (plot_param->values.size() != total_items) {
        const std::string error_message { gettext(
            "The product of rows and cols of the heatmap  must "
            "be equal to the size of values! ") };
        std::cerr << error_message << "\n";
        return false;
    }

    ImVec2 plot_size = ImGui::GetContentRegionAvail();
    ImPlotAxisFlags axes_flags = ImPlotAxisFlags_Lock
        | ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickMarks;
    ImPlotFlags plot_flags = ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText;

    if (ImPlot::BeginPlot(plot_param->main_plot_name.c_str(), plot_size,
                          plot_flags)) {
        ImPlot::SetupAxes(nullptr, nullptr, axes_flags, axes_flags);
        if (!plot_param->x_labels.empty()) {
            if (static_cast<int>(plot_param->x_labels.size())
                != plot_param->cols) {
                std::cerr << gettext(
                    "Size of a heatmap x_labels must be equal to cols "
                    "amount.")
                          << "\n";
                ImPlot::EndPlot();
                return false;
            }
            ImPlot::SetupAxisTicks(ImAxis_X1, 0 + 1.0 / total_items,
                                   1 - 1.0 / total_items, plot_param->cols,
                                   plot_param->x_labels.data());
        }
        if (!plot_param->y_labels.empty()) {
            if (static_cast<int>(plot_param->y_labels.size())
                != plot_param->rows) {
                std::cerr << gettext(
                    "Size of a heatmap y_labels must be equal to rows "
                    "amount.")
                          << "\n";
                ImPlot::EndPlot();
                return false;
            }
            ImPlot::SetupAxisTicks(ImAxis_Y1, 0 + 1.0 / total_items,
                                   1 - 1.0 / total_items, plot_param->rows,
                                   plot_param->y_labels.data());
        }

        ImPlot::PlotHeatmap("##NoTitle", plot_param->values.data(),
                            plot_param->rows, plot_param->cols,
                            plot_param->scale_min, plot_param->scale_max,
                            plot_param->label_fmt.c_str(),
                            plot_param->bounds_min, plot_param->bounds_max);

        ImPlot::EndPlot();
    } else {
        std::cerr << gettext("BeginPlot in Heatmaps is failed!") << "\n";
        return false;
    }

    return true;
}

auto ImGuiVulkanUtil::image_plot(
    const std::unique_ptr<VudiInit::ImagePlot>& plot_param) -> bool
{
    auto total_images_amount {
        plot_images_manager->get_plot_textures().size()
    };
    if (total_images_amount <= 0) {
        std::cerr << gettext(
            "Insufficient amout of images in plot images manager: ")
                  << total_images_amount << "\n";
        return false;
    }
    ImVec2 plot_size = ImGui::GetContentRegionAvail();

    ImPlotAxisFlags axes_flags
        = ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoDecorations;
    ImPlotFlags plot_flags = ImPlotFlags_NoMouseText | ImPlotFlags_NoLegend;
    if (ImPlot::BeginPlot(plot_param->main_plot_name.c_str(), plot_size,
                          plot_flags)) {
        double width_step { 0 };
        double height_step { 0 };
        uint32_t col { 0 };

        ImPlot::SetupAxes(nullptr, nullptr, axes_flags, axes_flags);

        ImPlot::SetupAxesLimits(0.0, plot_size.x, 0.0, plot_size.y,
                                ImGuiCond_Once);

        for (std::size_t i { 0 }; i < total_images_amount; ++i) {
            auto tex_id { (ImTextureID)(VkDescriptorSet)*plot_images_manager
                              ->get_plot_textures()[i]
                              .descriptor_set };

            const double image_y = plot_size.y - plot_param->images_height[i];
            auto bmin { ImPlotPoint(width_step + plot_param->hor_gap,
                                    image_y - height_step) };
            auto bmax { ImPlotPoint(plot_param->images_width[i] + width_step
                                        + plot_param->hor_gap,
                                    plot_size.y - height_step) };
            const std::string label
                = plot_param->image_labels.size() != total_images_amount
                ? ""
                : plot_param->image_labels[i];

            ImPlot::PlotImage("##NoTitle", tex_id, bmin, bmax);
            ImPlot::Annotation(plot_param->images_width[i] + width_step
                                   + plot_param->hor_gap,
                               plot_size.y - height_step, plot_param->label_bg,
                               ImVec2(-5, -2), false, "%s", label.c_str());

            width_step += plot_param->images_width[i] + plot_param->hor_gap;
            col += 1;
            if (plot_param->columns > 0 && col == plot_param->columns) {
                height_step
                    += plot_param->images_height[i] + plot_param->vert_gap;
                width_step = 0;
                col = 0;
            }
        }

        ImPlot::EndPlot();
    } else {
        std::cerr << gettext("BeginPlot in Image plot is failed!") << "\n";
        return false;
    }
    return true;
}

void ImGuiVulkanUtil::update_buffers(uint32_t frame_index)
{
    const auto& memory_properties {
        gen_presentation->get_physical_device().getMemoryProperties()
    };
    const auto& logical_device { gen_presentation->get_logical_device() };

    auto buffer_utils { std::make_unique<BufferUtils>(
        memory_properties, logical_device,
        gen_presentation->get_queue_index()) };

    ImDrawData* draw_data = ImGui::GetDrawData();
    if (!draw_data || draw_data->CmdListsCount == 0) {
        return;
    }

    // Calculate required buffer sizes
    vk::DeviceSize vertex_buffer_size
        = draw_data->TotalVtxCount * sizeof(ImDrawVert);
    vk::DeviceSize index_buffer_size
        = draw_data->TotalIdxCount * sizeof(ImDrawIdx);

    if (frame_index >= vertex_counts.size())
        return; // Safety

    // Resize buffers if needed
    if (static_cast<uint32_t>(draw_data->TotalVtxCount)
        > vertex_counts[frame_index]) {
        // Recreate vertex buffer with new size
        vertex_buffers[frame_index] = vk::raii::Buffer(nullptr);
        vertex_buffer_memories[frame_index] = vk::raii::DeviceMemory(nullptr);
        auto [vertex_buffer, vertex_buffer_memory]
            = buffer_utils->create_buffer(
                vertex_buffer_size, vk::BufferUsageFlagBits::eVertexBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible
                    | vk::MemoryPropertyFlagBits::eHostCoherent);
        vertex_buffers[frame_index] = std::move(vertex_buffer);
        vertex_buffer_memories[frame_index] = std::move(vertex_buffer_memory);

        vertex_counts[frame_index] = draw_data->TotalVtxCount;
    }

    if (static_cast<uint32_t>(draw_data->TotalIdxCount)
        > index_counts[frame_index]) {
        // Recreate index buffer with new size
        //        create_index_buffers(index_buffer_size);
        auto [index_buffer, index_buffer_memory] = buffer_utils->create_buffer(
            index_buffer_size, vk::BufferUsageFlagBits::eIndexBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible
                | vk::MemoryPropertyFlagBits::eHostCoherent);
        index_buffers[frame_index] = std::move(index_buffer);
        index_buffer_memories[frame_index] = std::move(index_buffer_memory);

        index_counts[frame_index] = draw_data->TotalIdxCount;
    }

    // Upload data to buffers
    ImDrawVert* vtx_dst = static_cast<ImDrawVert*>(
        vertex_buffer_memories[frame_index].mapMemory(0, vertex_buffer_size));
    ImDrawIdx* idx_dst = static_cast<ImDrawIdx*>(
        index_buffer_memories[frame_index].mapMemory(0, index_buffer_size));

    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        memcpy(vtx_dst, cmd_list->VtxBuffer.Data,
               cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_dst, cmd_list->IdxBuffer.Data,
               cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_dst += cmd_list->VtxBuffer.Size;
        idx_dst += cmd_list->IdxBuffer.Size;
    }

    vertex_buffer_memories[frame_index].unmapMemory();
    index_buffer_memories[frame_index].unmapMemory();
}

void ImGuiVulkanUtil::transition_image_layout(
    vk::Image image, vk::ImageLayout old_layout, vk::ImageLayout new_layout,
    vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
    vk::PipelineStageFlags2 src_stage_mask,
    vk::PipelineStageFlags2 dst_stage_mask,
    vk::ImageAspectFlags image_aspect_flags,
    vk::raii::CommandBuffer& command_buffer)
{
    vk::ImageMemoryBarrier2 barrier = {
        .srcStageMask = src_stage_mask,
        .srcAccessMask = src_access_mask,
        .dstStageMask = dst_stage_mask,
        .dstAccessMask = dst_access_mask,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = image,
        .subresourceRange = { .aspectMask = image_aspect_flags,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1 }
    };
    vk::DependencyInfo dependency_info = { .dependencyFlags = {},
                                           .imageMemoryBarrierCount = 1,
                                           .pImageMemoryBarriers = &barrier };
    command_buffer.pipelineBarrier2(dependency_info);
}

void ImGuiVulkanUtil::record_command_buffer(
    std::vector<vk::raii::CommandBuffer>& command_buffers,
    const uint32_t image_index, const uint32_t frame_index)
{
    update_buffers(frame_index);
    auto& command_buffer = command_buffers[frame_index];
    command_buffer.begin({});
    ImDrawData* draw_data = ImGui::GetDrawData();

    if (draw_data && draw_data->Textures) {
        // Process dynamic texture updates (v1.92+ RendererHasTextures
        // protocol)

        for (int n = 0; n < draw_data->Textures->Size; n++) {
            ImTextureData* tex = (*draw_data->Textures)[n];
            if (tex->Status != ImTextureStatus_OK) {
                update_texture(tex);
            }
        }
    }

    // Before starting rendering, transition the swapchain image to
    // vk::ImageLayout::eColorAttachmentOptimal
    transition_image_layout(
        gen_presentation->get_swapchain_images()[image_index],
        vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
        {}, // srcAccessMask (no need to wait for previous operations)
        vk::AccessFlagBits2::eColorAttachmentWrite, // dstAccessMask
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, // dstStage
        vk::ImageAspectFlagBits::eColor, command_buffer);

    vk::ClearValue clear_color { vk::ClearColorValue {
        std::array<float, 4> { 0.1f, 0.0f, 0.1f, 1.0f } } };

    vk::RenderingAttachmentInfo color_attachment_info {
        .imageView
        = gen_presentation->get_swapchain_image_views()[image_index],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = clear_color
    };
    const auto extent = gen_presentation->get_swapchain_extent();

    vk::RenderingInfo rendering_info {
        .renderArea = vk::Rect2D { { 0, 0 }, extent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_info
    };

    command_buffer.beginRendering(rendering_info);

    if (draw_data && draw_data->CmdListsCount > 0) {

        // Bind the pipeline used for ImGui
        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                    *pipeline);
        // Configure viewport for UI pixel coordinates
        vk::Viewport viewport { .width = static_cast<float>(extent.width),
                                .height = static_cast<float>(extent.height),
                                .minDepth = 0.0f,
                                .maxDepth = 1.0f };

        command_buffer.setViewport(0, viewport);

        // Convert from ImGui coordinates into NDC via a simple scale/translate
        push_const_block.scale = glm::vec2(2.0f / draw_data->DisplaySize.x,
                                           2.0f / draw_data->DisplaySize.y);
        push_const_block.translate = glm::vec2(-1.0f);
        command_buffer.pushConstants(
            *pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0,
            sizeof(PushConstBlock), &push_const_block);

        // We already filled these buffers this frame
        vk::DeviceSize offsets[] = { 0 };
        command_buffer.bindVertexBuffers(0, *vertex_buffers[frame_index],
                                         offsets);
        command_buffer.bindIndexBuffer(*index_buffers[frame_index], 0,
                                       vk::IndexType::eUint16);

        // Render command lists
        int vertex_offset = 0;
        int index_offset = 0;

        for (int i = 0; i < draw_data->CmdLists.Size; i++) {
            const ImDrawList* cmd_list = draw_data->CmdLists[i];

            for (int j = 0; j < cmd_list->CmdBuffer.Size; j++) {
                const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];

                // Set scissor rectangle
                vk::Rect2D scissor;
                scissor.offset.x
                    = std::max(static_cast<int32_t>(pcmd->ClipRect.x), 0);
                scissor.offset.y
                    = std::max(static_cast<int32_t>(pcmd->ClipRect.y), 0);
                scissor.extent.width = static_cast<uint32_t>(
                    pcmd->ClipRect.z - pcmd->ClipRect.x);
                scissor.extent.height = static_cast<uint32_t>(
                    pcmd->ClipRect.w - pcmd->ClipRect.y);
                command_buffer.setScissor(0, { scissor });

                VkDescriptorSet tex_handle = (VkDescriptorSet)pcmd->GetTexID();

                if (tex_handle) {
                    command_buffer.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics, *pipeline_layout, 0,
                        { vk::DescriptorSet(tex_handle) }, {});
                } else {
                    command_buffer.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics, *pipeline_layout, 0,
                        { *font_descriptor_set }, {});
                }

                // Draw
                command_buffer.drawIndexed(pcmd->ElemCount, 1, index_offset,
                                           vertex_offset, 0);
                index_offset += pcmd->ElemCount;
            }

            vertex_offset += cmd_list->VtxBuffer.Size;
        }
    }

    // Close the rendering scope for the UI overlay
    command_buffer.endRendering();

    if (save_image_config.save_current_image
        && !save_image_config.screenshot_path.empty()) {
        // Copying screenshot image from swapchain to buffer
        transition_image_layout(
            gen_presentation->get_swapchain_images()[image_index],
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::eTransferSrcOptimal,
            vk::AccessFlagBits2::eColorAttachmentWrite, // wait for Color write
            vk::AccessFlagBits2::eTransferRead, // dstAccessMask
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
            vk::PipelineStageFlagBits2::eTransfer, // dstStage
            vk::ImageAspectFlagBits::eColor, command_buffer);

        vk::BufferImageCopy region {
            .bufferOffset = 0,
            .bufferRowLength = 0, // tightly packed
            .bufferImageHeight = 0, // tightly packed

            .imageSubresource
            = { .aspectMask = vk::ImageAspectFlagBits::eColor,
               .mipLevel = 0,
               .baseArrayLayer = 0,
               .layerCount = 1 },

            .imageOffset = { 0, 0, 0 },
            .imageExtent = { extent.width, extent.height, 1 }
        };

        command_buffer.copyImageToBuffer(
            gen_presentation->get_swapchain_images()[image_index],
            vk::ImageLayout::eTransferSrcOptimal, screenshot_buffer, region);

        transition_image_layout(
            gen_presentation->get_swapchain_images()[image_index],
            vk::ImageLayout::eTransferSrcOptimal,
            vk::ImageLayout::ePresentSrcKHR,

            vk::AccessFlagBits2::eTransferRead, {},

            vk::PipelineStageFlagBits2::eTransfer,
            vk::PipelineStageFlagBits2::eBottomOfPipe,

            vk::ImageAspectFlagBits::eColor, command_buffer);

    } else {
        // After rendering, transition the swapchain image to
        // vk::ImageLayout::ePresentSrcKHR
        transition_image_layout(
            gen_presentation->get_swapchain_images()[image_index],
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR,
            vk::AccessFlagBits2::eColorAttachmentWrite, // srcAccessMask
            {}, // dstAccessMask
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
            vk::PipelineStageFlagBits2::eBottomOfPipe, // dstStage
            vk::ImageAspectFlagBits::eColor, command_buffer);
    }

    command_buffer.end();
}

void ImGuiVulkanUtil::create_semaphores_and_fences()
{
    assert(present_complete_semaphore.empty()
           && render_finished_semaphore.empty() && draw_fence.empty());

    for (std::size_t i { 0 };
         i < gen_presentation->get_swapchain_images().size(); i++) {
        render_finished_semaphore.emplace_back(
            gen_presentation->get_logical_device(), vk::SemaphoreCreateInfo());
    }

    for (uint32_t i { 0 }; i < max_frames_in_flight; i++) {
        present_complete_semaphore.emplace_back(
            gen_presentation->get_logical_device(), vk::SemaphoreCreateInfo());
        draw_fence.emplace_back(
            gen_presentation->get_logical_device(),
            vk::FenceCreateInfo { .flags
                                  = vk::FenceCreateFlagBits::eSignaled });
    }
}

auto ImGuiVulkanUtil::draw_frame(
    std::vector<vk::raii::CommandBuffer>& command_buffers,
    uint32_t& frame_index, bool& framebuffer_resized) -> bool
{
    auto fence_result = gen_presentation->get_logical_device().waitForFences(
        *draw_fence[frame_index], vk::True, UINT64_MAX);
    if (fence_result != vk::Result::eSuccess) {
        std::cerr << gettext("Failed to wait for fence!") << "\n";
        return false;
    }

    auto [result, image_index]
        = gen_presentation->get_swapchain().acquireNextImage(
            UINT64_MAX, *present_complete_semaphore[frame_index], nullptr);

    // Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined,
    // eErrorOutOfDateKHR can be checked as a result here and does not need to
    // be caught by an exception.
    if (result == vk::Result::eErrorOutOfDateKHR) {

        gen_presentation->recreate_swapchain();

        ImGuiIO& io = ImGui::GetIO();
        int width, height;

        if (!SDL_GetWindowSizeInPixels(gen_presentation->get_sdl_window(),
                                       &width, &height)) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                         gettext("SDL_GetWindowSizeInPixels failed: %s"),
                         SDL_GetError());
            std::cerr << gettext("SDL get window size failed.") << "\n";
            return false;
        }

        io.DisplaySize
            = ImVec2(static_cast<float>(width), static_cast<float>(height));
        screenshot_buffer.clear();
        screenshot_buffer_memory.clear();
        std::tie(screenshot_buffer, screenshot_buffer_memory)
            = create_screenshot_buffer_and_memory();
        return true;
    }
    // On other success codes than eSuccess and eSuboptimalKHR we just throw an
    // exception. On any error code, aquireNextImage already threw an
    // exception.
    if (result != vk::Result::eSuccess
        && result != vk::Result::eSuboptimalKHR) {
        assert(result == vk::Result::eTimeout
               || result == vk::Result::eNotReady);
        std::cerr << gettext("Failed to acquire swap chain image!") << "\n";
        return false;
    }

    gen_presentation->get_logical_device().resetFences(
        *draw_fence[frame_index]);

    command_buffers[frame_index].reset();
    record_command_buffer(command_buffers, image_index, frame_index);

    vk::PipelineStageFlags wait_destination_stage_mask(
        vk::PipelineStageFlagBits::eColorAttachmentOutput);
    const vk::SubmitInfo submit_info {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*present_complete_semaphore[frame_index],
        .pWaitDstStageMask = &wait_destination_stage_mask,
        .commandBufferCount = 1,
        .pCommandBuffers = &*command_buffers[frame_index],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &*render_finished_semaphore[image_index]
    };
    gen_presentation->get_presentation_queue().submit(
        submit_info, *draw_fence[frame_index]);

    if (save_image_config.save_current_image
        && !save_image_config.screenshot_path.empty()) {
        screenshot_is_saved = save_screenshot_buffer_to_file(frame_index);
    }

    const vk::PresentInfoKHR present_info_KHR {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*render_finished_semaphore[image_index],
        .swapchainCount = 1,
        .pSwapchains = &*gen_presentation->get_swapchain(),
        .pImageIndices = &image_index
    };
    result = gen_presentation->get_presentation_queue().presentKHR(
        present_info_KHR);

    // Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined,
    // eErrorOutOfDateKHR can be checked as a result here and does not need to
    // be caught by an exception.
    if ((result == vk::Result::eSuboptimalKHR)
        || (result == vk::Result::eErrorOutOfDateKHR) || framebuffer_resized) {
        framebuffer_resized = false;
        gen_presentation->recreate_swapchain();

        ImGuiIO& io = ImGui::GetIO();
        int width, height;
        SDL_GetWindowSizeInPixels(gen_presentation->get_sdl_window(), &width,
                                  &height);

        io.DisplaySize
            = ImVec2(static_cast<float>(width), static_cast<float>(height));

        screenshot_buffer.clear();
        screenshot_buffer_memory.clear();
        std::tie(screenshot_buffer, screenshot_buffer_memory)
            = create_screenshot_buffer_and_memory();

    } else {
        // There are no other success codes than eSuccess; on any error code,
        // presentKHR already threw an exception.
        assert(result == vk::Result::eSuccess);
    }

    frame_index = (frame_index + 1) % max_frames_in_flight;

    return true;
}

auto ImGuiVulkanUtil::save_screenshot_buffer_to_file(uint32_t frame_index)
    -> bool
{
    // Saving buffer with screenshot image to a png file
    auto screenshot_result
        = gen_presentation->get_logical_device().waitForFences(
            *draw_fence[frame_index], vk::True, UINT64_MAX);

    if (screenshot_result != vk::Result::eSuccess) {
        std::cerr << gettext("Screenshot fence wait failed!") << "\n";
        return false;
    }

    const vk::Extent2D extent = gen_presentation->get_swapchain_extent();

    const vk::DeviceSize buffer_size
        = vk::DeviceSize(extent.width) * vk::DeviceSize(extent.height) * 4;

    void* mapped_buffer = screenshot_buffer_memory.mapMemory(0, buffer_size);

    unsigned char* image_pixels = static_cast<unsigned char*>(mapped_buffer);

    std::vector<unsigned char> rgba(static_cast<size_t>(extent.width)
                                    * static_cast<size_t>(extent.height) * 4);

    const vk::Format format
        = gen_presentation->get_swapchain_surface_format().format;

    // Converting buffer to RGBA format if needed
    for (uint32_t y = 0; y < extent.height; ++y) {
        for (uint32_t x = 0; x < extent.width; ++x) {
            const size_t i = (static_cast<size_t>(y) * extent.width + x) * 4;
            if (format == vk::Format::eB8G8R8A8Unorm
                || format == vk::Format::eB8G8R8A8Srgb) {

                rgba[i + 0] = image_pixels[i + 2];
                rgba[i + 1] = image_pixels[i + 1];
                rgba[i + 2] = image_pixels[i + 0];

            } else if (format == vk::Format::eR8G8B8A8Unorm
                       || format == vk::Format::eR8G8B8A8Srgb) {

                rgba[i + 0] = image_pixels[i + 0];
                rgba[i + 1] = image_pixels[i + 1];
                rgba[i + 2] = image_pixels[i + 2];

            } else {
                std::cerr << gettext(
                    "Unsupported swapchain format for screenshot")
                          << "\n";
                return false;
            }

            rgba[i + 3] = 255;
        }
    }

    std::vector<unsigned char> png;

    unsigned error
        = lodepng::encode(png, rgba, extent.width, extent.height, LCT_RGBA, 8);
    if (!error) {
        lodepng::save_file(png, save_image_config.screenshot_path);
    } else {
        const std::string error_message {
            gettext("Lodepng image encoder error ") + std::to_string(error)
            + " : " + lodepng_error_text(error)
        };
        std::cerr << error_message << "\n";
        return false;
    }

    screenshot_buffer_memory.unmapMemory();
    save_image_config.save_current_image = false;
    return true;
}

void ImGuiVulkanUtil::handle_key_press(SDL_Event& event, bool& is_running,
                                       bool& framebuffer_resized)
{

    while (SDL_PollEvent(&event)) {

        switch (event.type) {
        case SDL_EVENT_QUIT: {
            is_running = false;
            break;
        }
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
            is_running = false;
            break;
        }
        case SDL_EVENT_WINDOW_RESIZED: {
            framebuffer_resized = true;
            break;
        }
        case SDL_EVENT_KEY_DOWN: {

            if (!event.key.repeat && event.key.scancode == SDL_SCANCODE_S
                && (event.key.mod & SDL_KMOD_ALT)) {
                style_num = static_cast<GuiStyles>(
                    (static_cast<int>(style_num) + 1)
                    % static_cast<int>(GuiStyles::Items_count));
                theme_button = static_cast<int>(style_num);
                set_style(style_num);
                break;
            }

            if (!event.key.repeat && event.key.scancode == SDL_SCANCODE_Q
                && (event.key.mod & SDL_KMOD_CTRL)) {
                is_running = false;
                break;
            }

            if (!event.key.repeat && event.key.scancode == SDL_SCANCODE_Z
                && (event.key.mod & SDL_KMOD_ALT)) {
                ImPlot::SetNextAxesToFit();
                break;
            }

            if (!event.key.repeat && event.key.scancode == SDL_SCANCODE_F1) {
                imgui_demo_state.show_imgui_about_window
                    = imgui_demo_state.show_imgui_about_window == true ? false
                                                                       : true;
                break;
            }

            if (!event.key.repeat && event.key.scancode == SDL_SCANCODE_S
                && (event.key.mod & SDL_KMOD_CTRL)) {
                save_screenshot_dialog();
                break;
            }

            if (!event.key.repeat && event.key.scancode == SDL_SCANCODE_F
                && (event.key.mod & SDL_KMOD_CTRL)) {
                const bool maximized
                    = (SDL_GetWindowFlags(gen_presentation->get_sdl_window())
                       & SDL_WINDOW_MAXIMIZED)
                    != 0;

                const bool success = maximized
                    ? SDL_RestoreWindow(gen_presentation->get_sdl_window())
                    : SDL_MaximizeWindow(gen_presentation->get_sdl_window());

                if (!success) {
                    SDL_Log(gettext("Failed to change window state: %s"),
                            SDL_GetError());
                    throw std::runtime_error(
                        gettext("Failed to change SDL window state."));
                }
            }
            break;
        }

        case SDL_EVENT_MOUSE_MOTION: {
            ImGuiIO& io = ImGui::GetIO();
            io.AddMousePosEvent(static_cast<float>(event.motion.x),
                                static_cast<float>(event.motion.y));
            break;
        }

        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            ImGuiIO& io = ImGui::GetIO();

            if (event.button.button == SDL_BUTTON_LEFT)
                io.AddMouseButtonEvent(0, true);

            if (event.button.button == SDL_BUTTON_RIGHT)
                io.AddMouseButtonEvent(1, true);

            if (event.button.button == SDL_BUTTON_MIDDLE)
                io.AddMouseButtonEvent(2, true);

            break;
        }

        case SDL_EVENT_MOUSE_BUTTON_UP: {
            ImGuiIO& io = ImGui::GetIO();

            if (event.button.button == SDL_BUTTON_LEFT)
                io.AddMouseButtonEvent(0, false);

            if (event.button.button == SDL_BUTTON_RIGHT)
                io.AddMouseButtonEvent(1, false);

            if (event.button.button == SDL_BUTTON_MIDDLE)
                io.AddMouseButtonEvent(2, false);

            break;
        }

        case SDL_EVENT_MOUSE_WHEEL: {
            ImGuiIO& io = ImGui::GetIO();

            io.AddMouseWheelEvent(static_cast<float>(event.wheel.x),
                                  static_cast<float>(event.wheel.y));

            break;
        }
        }
    }
}

void ImGuiVulkanUtil::clean_up()
{
    if (implot_context) {
        ImPlot::DestroyContext(implot_context);
        implot_context = nullptr;
    }

    if (imgui_context) {
        ImGui::DestroyContext(imgui_context);
        imgui_context = nullptr;
    }
}

void ImGuiVulkanUtil::update_texture(ImTextureData* tex)
{
    if (tex->Status == ImTextureStatus_WantCreate
        || tex->Status == ImTextureStatus_WantUpdates) {
        int tex_width = tex->Width;
        int tex_height = tex->Height;
        unsigned char* tex_data = (unsigned char*)tex->Pixels;

        if (!tex_data) {
            return;
        }

        vk::DeviceSize upload_size
            = tex_width * tex_height * tex->BytesPerPixel;

        try {
            if (tex->Status == ImTextureStatus_WantCreate) {
                vk::Extent2D extent {
                    static_cast<uint32_t>(tex_width),
                    static_cast<uint32_t>(tex_height),
                };

                vk::Format format = (tex->BytesPerPixel == 4)
                    ? vk::Format::eR8G8B8A8Unorm
                    : vk::Format::eR8Unorm;
                auto [cur_tex_image, cur_tex_image_memory]
                    = create_image(extent, format);
                auto cur_tex_image_view
                    = create_image_view(cur_tex_image, format);
                auto cur_tex_sampler = create_sampler();

                auto cur_tex_descriptor_set = create_descriptor_set(
                    font_descriptor_set_layout, draw_tex_descriptor_pool);

                icon_textures[tex]
                    = { .image = std::move(cur_tex_image),
                        .image_memory = std::move(cur_tex_image_memory),
                        .image_view = std::move(cur_tex_image_view),
                        .sampler = std::move(cur_tex_sampler),
                        .descriptor_set = std::move(cur_tex_descriptor_set)

                      };

                update_descriptor_set(icon_textures[tex].sampler,
                                      icon_textures[tex].image_view,
                                      icon_textures[tex].descriptor_set);
            }

            create_staging_buffer(upload_size, tex_data, tex_width, tex_height,
                                  icon_textures[tex].image);

            // Store descriptor set handle as the ImTextureID
            tex->SetTexID(
                (ImTextureID)(intptr_t)(VkDescriptorSet)*icon_textures[tex]
                    .descriptor_set);

            tex->SetStatus(ImTextureStatus_OK);
        }

        catch (const std::exception& e) {
            std::string error_message { gettext(
                "Faile to update ImGui texture: ") };
            error_message += e.what();
            throw std::runtime_error(error_message);
        }
    }
}

void ImGuiVulkanUtil::save_file_callback(void* userdata,
                                         const char* const* filelist,
                                         int filter)
{
    if (!filelist) {
        SDL_Log(gettext("An error occured during saving file: %s"),
                SDL_GetError());
        return;
    } else if (!*filelist) {
        SDL_Log(gettext("The user did not select any file."));
        SDL_Log(gettext("Most likely, the dialog was canceled."));
        return;
    }

    auto* image_config = static_cast<SaveImageConfig*>(userdata);
    image_config->screenshot_path = filelist[0];
    image_config->save_current_image = true;
}

void ImGuiVulkanUtil::save_screenshot_dialog()
{
    auto time_now = std::chrono::floor<std::chrono::seconds>(
        std::chrono::system_clock::now());
    auto local_time = std::chrono::current_zone()->to_local(time_now);

    const std::string cur_time { std::format("_{:%d-%m-%Y_%H:%M:%S}",
                                             local_time) };
    const std::string filename { gettext(gettext("Vudiplot_screenshot")) };
    const std::string& cur_path { std::filesystem::current_path().string()
                                  + "/" + filename + cur_time + ".png" };

    SDL_ShowSaveFileDialog(save_file_callback, &save_image_config, nullptr,
                           save_filters, SDL_arraysize(save_filters),
                           cur_path.c_str());
}

void ImGuiVulkanUtil::save_screenshot_success_window(
    const std::string& modal_window_name)
{
    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    int width, height;
    if (!SDL_GetWindowSizeInPixels(gen_presentation->get_sdl_window(), &width,
                                   &height)) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     gettext("SDL_GetWindowSizeInPixels failed: %s"),
                     SDL_GetError());
        throw std::runtime_error(gettext(
            "SDL get window size failed in save screenshot success window."));
    }

    ImGui::SetNextWindowSizeConstraints(ImVec2(width / 2, 0),
                                        ImVec2(width, FLT_MAX));

    if (ImGui::BeginPopupModal(modal_window_name.c_str(), nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        const std::string saved_image_path {
            save_image_config.screenshot_path
        };
        const std::string text_message {
            gettext("Screenshot is saved to this path:\n") + saved_image_path
        };

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width * 0.75f);

        ImGui::TextWrapped("%s", text_message.c_str());

        ImGui::PopTextWrapPos();

        ImGui::Separator();

        ImVec2 OK_button_size { ImVec2(width * 0.75, 0) };
        center_widget_width_position(OK_button_size.x);
        if (ImGui::Button(gettext("OK"), OK_button_size)) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

auto ImGuiVulkanUtil::load_plot_images(
    const std::vector<std::vector<unsigned char>>& raw_images,
    const std::vector<std::size_t>& images_width,
    const std::vector<std::size_t>& images_height) -> bool
{
    auto new_images_amount { raw_images.size() };

    if (new_images_amount <= 0) {
        std::cerr << gettext("Insufficient amout of raw images: ")
                  << new_images_amount << "\n";
        return false;
    }

    if (images_height.size() != new_images_amount
        || images_width.size() != new_images_amount) {
        std::cerr << gettext("Amount of images and their heights "
                             "or/and widths amounts don't match!")
                  << "\n";
        return false;
    }

    total_draw_pool_size += new_images_amount;

    draw_tex_descriptor_pool.clear();

    draw_tex_descriptor_pool
        = create_descriptor_pool(total_draw_pool_size, total_draw_pool_size);

    plot_images_descriptor_pool
        = create_descriptor_pool(new_images_amount, new_images_amount);

    plot_images_manager = std::make_unique<ImGuiTextureManager>(
        gen_presentation->get_logical_device(), plot_images_descriptor_pool);

    for (std::size_t i { 0 }; i < new_images_amount; ++i) {

        auto mesh_info {
            std::make_unique<MeshInitInfoRawChar>(MeshInitInfoRawChar {
                .rgba_image = raw_images[i],
                .image_width = static_cast<uint32_t>(images_width[i]),
                .image_height = static_cast<uint32_t>(images_height[i]),
                .physical_device = gen_presentation->get_physical_device(),
                .logical_device = gen_presentation->get_logical_device(),
                .queue_index = gen_presentation->get_queue_index(),
                .graphics_queue = gen_presentation->get_presentation_queue() })
        };

        auto texture_mesh { std::make_unique<ImGuiTextureMesh>(mesh_info) };

        plot_images_manager->register_texture(
            texture_mesh->get_texture_image(),
            texture_mesh->get_texture_image_memory(),
            texture_mesh->get_texture_image_view(),
            texture_mesh->get_texture_sampler(),
            texture_mesh->get_image_size().image_width,
            texture_mesh->get_image_size().image_height);
    }

    return true;
}
