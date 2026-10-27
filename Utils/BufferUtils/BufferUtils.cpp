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

#include "BufferUtils.hpp"

BufferUtils::BufferUtils(
    const vk::PhysicalDeviceMemoryProperties& physical_mem_properties,
    const vk::raii::Device& logical_device, const uint32_t queue_index)
    : physical_mem_properties(physical_mem_properties)
    , logical_device(logical_device)
    , queue_index(queue_index)
{
    create_command_pool();
}

auto BufferUtils::find_memory_type(uint32_t memory_type_filter,
                                   vk::MemoryPropertyFlags memory_properties)
    -> uint32_t
{

    for (uint32_t i = 0; i < physical_mem_properties.memoryTypeCount; i++) {
        if ((memory_type_filter & (1u << i))
            && (physical_mem_properties.memoryTypes[i].propertyFlags
                & memory_properties)
                == memory_properties) {
            return i;
        }
    }

    throw std::runtime_error(gettext("Failed to find suitable memory type!"));
}

auto BufferUtils::create_buffer(vk::DeviceSize buffer_size,
                                vk::BufferUsageFlags buffer_usage,
                                vk::MemoryPropertyFlags mem_properties)
    -> std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>
{

    vk::BufferCreateInfo buffer_info { .size = buffer_size,
                                       .usage = buffer_usage,
                                       .sharingMode
                                       = vk::SharingMode::eExclusive };
    vk::raii::Buffer buffer = vk::raii::Buffer(logical_device, buffer_info);

    vk::MemoryRequirements mem_requirements = buffer.getMemoryRequirements();
    vk::MemoryAllocateInfo memory_allocate_info {
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex
        = find_memory_type(mem_requirements.memoryTypeBits, mem_properties)
    };

    vk::raii::DeviceMemory buffer_memory
        = vk::raii::DeviceMemory(logical_device, memory_allocate_info);

    buffer.bindMemory(*buffer_memory, 0);

    return { std::move(buffer), std::move(buffer_memory) };
}

void BufferUtils::create_command_pool()
{
    vk::CommandPoolCreateInfo pool_info {
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = queue_index
    };
    command_pool = vk::raii::CommandPool(logical_device, pool_info);
}

auto BufferUtils::begin_singletime_commands()
    -> std::unique_ptr<vk::raii::CommandBuffer>
{
    vk::CommandBufferAllocateInfo alloc_info {
        .commandPool = command_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1
    };

    std::unique_ptr<vk::raii::CommandBuffer> command_buffer
        = std::make_unique<vk::raii::CommandBuffer>(std::move(
            vk::raii::CommandBuffers(logical_device, alloc_info).front()));

    vk::CommandBufferBeginInfo begin_info {
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    };
    command_buffer->begin(begin_info);

    return command_buffer;
}

void BufferUtils::end_singletime_commands(
    const vk::raii::CommandBuffer& command_buffer,
    vk::raii::Queue& graphics_queue) const
{
    command_buffer.end();

    vk::SubmitInfo submit_info { .commandBufferCount = 1,
                                 .pCommandBuffers = &*command_buffer };
    graphics_queue.submit(submit_info, nullptr);
    graphics_queue.waitIdle();
}

auto BufferUtils::create_image(uint32_t width, uint32_t height,
                               vk::Format format, vk::ImageTiling tiling,
                               vk::ImageUsageFlags usage,
                               vk::MemoryPropertyFlags mem_properties,
                               const uint32_t mip_levels,
                               const vk::SampleCountFlagBits msaa_samples)
    -> std::pair<vk::raii::Image, vk::raii::DeviceMemory>
{
    vk::ImageCreateInfo image_info {
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = { width, height, 1 },
        .mipLevels = mip_levels,
        .arrayLayers = 1,
        .samples = msaa_samples,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive
    };

    vk::raii::Image image = vk::raii::Image(logical_device, image_info);

    vk::MemoryRequirements mem_requirements = image.getMemoryRequirements();
    vk::MemoryAllocateInfo alloc_info {
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex
        = find_memory_type(mem_requirements.memoryTypeBits, mem_properties)
    };
    vk::raii::DeviceMemory image_memory
        = vk::raii::DeviceMemory(logical_device, alloc_info);
    image.bindMemory(image_memory, 0);

    return { std::move(image), std::move(image_memory) };
}

auto BufferUtils::create_imageview(const vk::raii::Image& image,
                                   const vk::Format format,
                                   const vk::ImageAspectFlagBits aspect_mask,
                                   const uint32_t mip_levels)
    -> vk::raii::ImageView
{
    vk::ImageViewCreateInfo view_info {
        .image = image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = { .aspectMask = aspect_mask,
                             .baseMipLevel = 0,
                             .levelCount = mip_levels,
                             .baseArrayLayer = 0,
                             .layerCount = 1 }
    };
    vk::raii::ImageView image_view
        = vk::raii::ImageView(logical_device, view_info);
    return image_view;
}

auto BufferUtils::transition_image_layout(const vk::raii::Image& image,
                                          vk::ImageLayout old_layout,
                                          vk::ImageLayout new_layout,
                                          const uint32_t mip_levels,
                                          vk::raii::Queue& graphics_queue)
    -> bool
{
    const auto command_buffer { begin_singletime_commands() };
    vk::ImageMemoryBarrier barrier {
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = image,
        .subresourceRange = { .aspectMask = vk::ImageAspectFlagBits::eColor,
                             .levelCount = mip_levels,
                             .layerCount = 1 }
    };

    vk::PipelineStageFlags source_stage;
    vk::PipelineStageFlags destination_stage;

    if (old_layout == vk::ImageLayout::eUndefined
        && new_layout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        destination_stage = vk::PipelineStageFlagBits::eTransfer;
    } else if (old_layout == vk::ImageLayout::eTransferDstOptimal
               && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        source_stage = vk::PipelineStageFlagBits::eTransfer;
        destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
    } else {
        return false;
    }

    command_buffer->pipelineBarrier(source_stage, destination_stage, {}, {},
                                    {}, barrier);
    end_singletime_commands(*command_buffer, graphics_queue);

    return true;
}

void BufferUtils::copy_buffer_to_image(const vk::raii::Buffer& buffer,
                                       vk::raii::Image& image, uint32_t width,
                                       uint32_t height,
                                       vk::raii::Queue& graphics_queue)
{
    auto command_buffer { begin_singletime_commands() };
    vk::BufferImageCopy region {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = { .aspectMask = vk::ImageAspectFlagBits::eColor,
                             .mipLevel = 0,
                             .baseArrayLayer = 0,
                             .layerCount = 1 },
        .imageOffset = { 0, 0, 0 },
        .imageExtent = { width, height, 1 }
    };
    command_buffer->copyBufferToImage(
        buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
    end_singletime_commands(*command_buffer, graphics_queue);
}
