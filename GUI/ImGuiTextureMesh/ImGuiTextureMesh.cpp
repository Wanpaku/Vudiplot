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

#include "ImGuiTextureMesh.hpp"

#include <ktx.h>
#include <BufferUtils.hpp>

#include <fstream>
#include <iostream>
#include <memory>
#include <cmath>

ImGuiTextureMesh::ImGuiTextureMesh(
    const std::unique_ptr<MeshInitInfoFile>& mesh_init_info)
    : physical_device(mesh_init_info->physical_device)
    , logical_device(mesh_init_info->logical_device)
    , queue_index(mesh_init_info->queue_index)
    , graphics_queue(mesh_init_info->graphics_queue)
{
    auto [staging_buffer, staging_buffer_memory]
        = create_staging_buffer_from_file(mesh_init_info->texture_path);
    create_texture_image(std::move(staging_buffer),
                         std::move(staging_buffer_memory));
    create_texture_image_view();
    create_texture_sampler();
}

ImGuiTextureMesh::ImGuiTextureMesh(
    const std::unique_ptr<MeshInitInfoRawChar>& mesh_init_info)
    : physical_device(mesh_init_info->physical_device)
    , logical_device(mesh_init_info->logical_device)
    , queue_index(mesh_init_info->queue_index)
    , graphics_queue(mesh_init_info->graphics_queue)
{
    auto [staging_buffer, staging_buffer_memory]
        = create_staging_buffer_from_rgba(mesh_init_info->rgba_image,
                                          mesh_init_info->image_width,
                                          mesh_init_info->image_height);
    create_texture_image(std::move(staging_buffer),
                         std::move(staging_buffer_memory));
    create_texture_image_view();
    create_texture_sampler();
}

auto ImGuiTextureMesh::create_staging_buffer_from_file(
    const std::string& texture_path)
    -> std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>
{
    // Load KTX2 texture instead of using stb_image
    ktxTexture* ktx_texture;
    KTX_error_code result = ktxTexture_CreateFromNamedFile(
        texture_path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        &ktx_texture);

    if (result != KTX_SUCCESS) {
        throw std::runtime_error(gettext("Failed to load ktx texture image!"));
    }

    // Get texture dimensions and data
    texture_image_size.image_width = ktx_texture->baseWidth;
    texture_image_size.image_height = ktx_texture->baseHeight;
    const ktx_size_t image_size = ktxTexture_GetImageSize(ktx_texture, 0);
    ktx_uint8_t* ktx_texture_data = ktxTexture_GetData(ktx_texture);

    mip_levels = ktx_texture->numLevels;

    const auto memory_properties = physical_device.getMemoryProperties();

    auto buffer_utils { std::make_unique<BufferUtils>(
        memory_properties, logical_device, queue_index) };

    auto [staging_buffer, staging_buffer_memory] = buffer_utils->create_buffer(
        image_size, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible
            | vk::MemoryPropertyFlagBits::eHostCoherent);

    void* data = staging_buffer_memory.mapMemory(0, image_size);
    memcpy(data, ktx_texture_data, image_size);
    staging_buffer_memory.unmapMemory();

    ktxTexture_Destroy(ktx_texture);

    return std::make_pair(std::move(staging_buffer),
                          std::move(staging_buffer_memory));
}

auto ImGuiTextureMesh::create_staging_buffer_from_rgba(
    std::vector<unsigned char>& rgba_image, const uint32_t width,
    const uint32_t height)
    -> std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>
{
    texture_image_size.image_width = width;
    texture_image_size.image_height = height;
    const vk::DeviceSize image_size = width * height * 4;

    mip_levels
        = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height))))
        + 1;

    const auto memory_properties = physical_device.getMemoryProperties();

    auto buffer_utils { std::make_unique<BufferUtils>(
        memory_properties, logical_device, queue_index) };

    auto [staging_buffer, staging_buffer_memory] = buffer_utils->create_buffer(
        image_size, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible
            | vk::MemoryPropertyFlagBits::eHostCoherent);

    void* data = staging_buffer_memory.mapMemory(0, image_size);
    memcpy(data, rgba_image.data(), image_size);
    staging_buffer_memory.unmapMemory();

    return std::make_pair(std::move(staging_buffer),
                          std::move(staging_buffer_memory));
}

void ImGuiTextureMesh::create_texture_image(
    const vk::raii::Buffer staging_buffer,
    const vk::raii::DeviceMemory buffer_memory)
{
    const auto memory_properties = physical_device.getMemoryProperties();

    auto buffer_utils { std::make_unique<BufferUtils>(
        memory_properties, logical_device, queue_index) };

    std::tie(texture_image, texture_image_memory) = buffer_utils->create_image(
        texture_image_size.image_width, texture_image_size.image_height,
        vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferSrc
            | vk::ImageUsageFlagBits::eTransferDst
            | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal, mip_levels,
        vk::SampleCountFlagBits::e1);

    auto is_transition_ok { buffer_utils->transition_image_layout(
        texture_image, vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal, mip_levels, graphics_queue) };

    if (!is_transition_ok) {
        throw std::invalid_argument(gettext("Unsupported layout transition!"));
    }

    buffer_utils->copy_buffer_to_image(
        staging_buffer, texture_image, texture_image_size.image_width,
        texture_image_size.image_height, graphics_queue);

    generate_mipmaps(texture_image, vk::Format::eR8G8B8A8Srgb,
                     texture_image_size.image_width,
                     texture_image_size.image_height);
}

void ImGuiTextureMesh::generate_mipmaps(vk::raii::Image& image,
                                        vk::Format image_format,
                                        int32_t tex_width, int32_t tex_height)
{
    // Check if image format supports linear blit-ing
    vk::FormatProperties format_properties
        = physical_device.getFormatProperties(image_format);

    if (!(format_properties.optimalTilingFeatures
          & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
        throw std::runtime_error(
            gettext("Texture image format does not support linear blitting!"));
    }
    const auto memory_properties = physical_device.getMemoryProperties();

    auto buffer_utils { std::make_unique<BufferUtils>(
        memory_properties, logical_device, queue_index) };

    std::unique_ptr<vk::raii::CommandBuffer> command_buffer
        = buffer_utils->begin_singletime_commands();

    vk::ImageMemoryBarrier barrier
        = { .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = vk::AccessFlagBits::eTransferRead,
            .oldLayout = vk::ImageLayout::eTransferDstOptimal,
            .newLayout = vk::ImageLayout::eTransferSrcOptimal,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image = image };
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mip_width = tex_width;
    int32_t mip_height = tex_height;

    for (uint32_t i = 1; i < mip_levels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                        vk::PipelineStageFlagBits::eTransfer,
                                        {}, {}, {}, barrier);

        vk::ArrayWrapper1D<vk::Offset3D, 2> offsets, dst_offsets;
        offsets[0] = vk::Offset3D { 0, 0, 0 };
        offsets[1] = vk::Offset3D { mip_width, mip_height, 1 };
        dst_offsets[0] = vk::Offset3D { 0, 0, 0 };
        dst_offsets[1]
            = vk::Offset3D { mip_width > 1 ? mip_width / 2 : 1,
                             mip_height > 1 ? mip_height / 2 : 1, 1 };
        vk::ImageBlit blit = { .srcSubresource = {},
                               .srcOffsets = offsets,
                               .dstSubresource = {},
                               .dstOffsets = dst_offsets };
        blit.srcSubresource
            = vk::ImageSubresourceLayers { vk::ImageAspectFlagBits::eColor,
                                           i - 1, 0, 1 };
        blit.dstSubresource
            = vk::ImageSubresourceLayers { vk::ImageAspectFlagBits::eColor, i,
                                           0, 1 };

        command_buffer->blitImage(image, vk::ImageLayout::eTransferSrcOptimal,
                                  image, vk::ImageLayout::eTransferDstOptimal,
                                  { blit }, vk::Filter::eLinear);

        barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        command_buffer->pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

        if (mip_width > 1)
            mip_width /= 2;
        if (mip_height > 1)
            mip_height /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mip_levels - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                    vk::PipelineStageFlagBits::eFragmentShader,
                                    {}, {}, {}, barrier);

    buffer_utils->end_singletime_commands(*command_buffer, graphics_queue);
}

void ImGuiTextureMesh::create_texture_image_view()
{
    const auto memory_properties = physical_device.getMemoryProperties();

    auto buffer_utils { std::make_unique<BufferUtils>(
        memory_properties, logical_device, queue_index) };

    texture_image_view = buffer_utils->create_imageview(
        texture_image, vk::Format::eR8G8B8A8Srgb,
        vk::ImageAspectFlagBits::eColor, mip_levels);
}

void ImGuiTextureMesh::create_texture_sampler()
{
    vk::PhysicalDeviceProperties physical_device_properties
        = physical_device.getProperties();
    vk::SamplerCreateInfo sampler_info {
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eRepeat,
        .addressModeV = vk::SamplerAddressMode::eRepeat,
        .addressModeW = vk::SamplerAddressMode::eRepeat,
        .mipLodBias = 0.0f,
        .anisotropyEnable = vk::True,
        .maxAnisotropy
        = physical_device_properties.limits.maxSamplerAnisotropy,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = 0.0f,
        .maxLod = vk::LodClampNone
    };
    texture_sampler = vk::raii::Sampler(logical_device, sampler_info);
}

auto ImGuiTextureMesh::get_texture_image() -> vk::raii::Image
{
    return std::move(texture_image);
}

auto ImGuiTextureMesh::get_texture_image_memory() -> vk::raii::DeviceMemory
{
    return std::move(texture_image_memory);
}

auto ImGuiTextureMesh::get_texture_image_view() -> vk::raii::ImageView
{
    return std::move(texture_image_view);
}

auto ImGuiTextureMesh::get_texture_sampler() -> vk::raii::Sampler
{
    return std::move(texture_sampler);
}

auto ImGuiTextureMesh::get_image_size() const -> ImageSize
{
    return texture_image_size;
}
