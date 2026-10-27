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

// This class creates necessary command buffers
// for recording commands for drawing images on the screen.

#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <vector>

class GenCommandBuffer {
public:
    GenCommandBuffer(const uint32_t queue_index,
                     const vk::raii::Device& logical_device,
                     const uint32_t max_frame_in_flight);

    [[nodiscard]] auto get_command_buffers()
        -> std::vector<vk::raii::CommandBuffer>&;
    [[nodiscard]] auto get_max_frame_in_flight() const -> uint32_t;

private:
    const uint32_t queue_index;
    const vk::raii::Device& logical_device;
    vk::raii::CommandPool command_pool = nullptr;
    std::vector<vk::raii::CommandBuffer> command_buffers;
    const uint32_t max_frame_in_flight;

    void create_command_pool();
    void create_command_buffers();
};
