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

// This class creates necessary utilities for routine operations
// such as buffer and image creation.

#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

class BufferUtils {
public:
    BufferUtils(
        const vk::PhysicalDeviceMemoryProperties& physical_mem_properties,
        const vk::raii::Device& logical_device, const uint32_t queue_index);

    [[nodiscard]] auto create_buffer(vk::DeviceSize buffer_size,
                                     vk::BufferUsageFlags buffer_usage,
                                     vk::MemoryPropertyFlags mem_properties)
        -> std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>;

    [[nodiscard]] auto begin_singletime_commands()
        -> std::unique_ptr<vk::raii::CommandBuffer>;
    void end_singletime_commands(const vk::raii::CommandBuffer& command_buffer,
                                 vk::raii::Queue& graphics_queue) const;

    [[nodiscard]] auto create_image(uint32_t width, uint32_t height,
                                    vk::Format format, vk::ImageTiling tiling,
                                    vk::ImageUsageFlags usage,
                                    vk::MemoryPropertyFlags mem_properties,
                                    const uint32_t mip_levels,
                                    const vk::SampleCountFlagBits msaa_samples)
        -> std::pair<vk::raii::Image, vk::raii::DeviceMemory>;

    [[nodiscard]] auto
    create_imageview(const vk::raii::Image& image, const vk::Format format,
                     const vk::ImageAspectFlagBits aspect_mask,

                     const uint32_t mip_levels) -> vk::raii::ImageView;

    [[nodiscard]] auto transition_image_layout(const vk::raii::Image& image,
                                               vk::ImageLayout old_layout,
                                               vk::ImageLayout new_layout,
                                               const uint32_t mip_levels,
                                               vk::raii::Queue& graphics_queue)
        -> bool;
    void copy_buffer_to_image(const vk::raii::Buffer& buffer,
                              vk::raii::Image& image, uint32_t width,
                              uint32_t height,
                              vk::raii::Queue& graphics_queue);

private:
    const vk::PhysicalDeviceMemoryProperties& physical_mem_properties;
    const vk::raii::Device& logical_device;
    const uint32_t queue_index;
    vk::raii::CommandPool command_pool { nullptr };

    void create_command_pool();
    [[nodiscard]] auto
    find_memory_type(uint32_t memory_type_filter,
                     vk::MemoryPropertyFlags memory_properties) -> uint32_t;
};
