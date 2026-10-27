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

#include "GenCommandBuffer.hpp"

GenCommandBuffer::GenCommandBuffer(const uint32_t queue_index,
                                   const vk::raii::Device& logical_device,
                                   const uint32_t max_frame_in_flight)
    : queue_index(queue_index)
    , logical_device(logical_device)
    , max_frame_in_flight(max_frame_in_flight)
{
    create_command_pool();
    create_command_buffers();
}

void GenCommandBuffer::create_command_pool()
{
    vk::CommandPoolCreateInfo pool_info {
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = queue_index
    };
    command_pool = vk::raii::CommandPool(logical_device, pool_info);
}

void GenCommandBuffer::create_command_buffers()
{
    vk::CommandBufferAllocateInfo alloc_info {
        .commandPool = command_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = max_frame_in_flight
    };
    command_buffers = vk::raii::CommandBuffers(logical_device, alloc_info);
}

auto GenCommandBuffer::get_command_buffers()
    -> std::vector<vk::raii::CommandBuffer>&
{
    return command_buffers;
}

auto GenCommandBuffer::get_max_frame_in_flight() const -> uint32_t
{
    return max_frame_in_flight;
}
