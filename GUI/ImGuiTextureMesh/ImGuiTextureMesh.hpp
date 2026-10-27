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

// This class creates a mesh for loading images
// specifically for Dear ImGui needs.
// For an image loading was used KTX library.

#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

struct MeshInitInfoFile {
    const std::string texture_path;
    const vk::raii::PhysicalDevice& physical_device { nullptr };
    const vk::raii::Device& logical_device { nullptr };
    const uint32_t queue_index;
    vk::raii::Queue& graphics_queue;
};

struct MeshInitInfoRawChar {
    std::vector<unsigned char> rgba_image;
    const uint32_t image_width;
    const uint32_t image_height;
    const vk::raii::PhysicalDevice& physical_device { nullptr };
    const vk::raii::Device& logical_device { nullptr };
    const uint32_t queue_index;
    vk::raii::Queue& graphics_queue;
};

struct ImageSize {
    uint32_t image_width;
    uint32_t image_height;
};

class ImGuiTextureMesh {
public:
    explicit ImGuiTextureMesh(
        const std::unique_ptr<MeshInitInfoFile>& mesh_init_info);
    explicit ImGuiTextureMesh(
        const std::unique_ptr<MeshInitInfoRawChar>& mesh_init_info);

    [[nodiscard]] auto get_texture_image() -> vk::raii::Image;
    [[nodiscard]] auto get_texture_image_memory() -> vk::raii::DeviceMemory;
    [[nodiscard]] auto get_texture_image_view() -> vk::raii::ImageView;
    [[nodiscard]] auto get_texture_sampler() -> vk::raii::Sampler;
    [[nodiscard]] auto get_image_size() const -> ImageSize;

private:
    const vk::raii::PhysicalDevice& physical_device;
    const vk::raii::Device& logical_device;

    const uint32_t queue_index;
    uint32_t mip_levels;
    vk::raii::Queue& graphics_queue;

    vk::raii::Image texture_image { nullptr };
    vk::raii::DeviceMemory texture_image_memory { nullptr };

    vk::raii::ImageView texture_image_view { nullptr };
    vk::raii::Sampler texture_sampler { nullptr };

    ImageSize texture_image_size;

    void generate_mipmaps(vk::raii::Image& image, vk::Format image_format,
                          int32_t tex_width, int32_t tex_height);

    auto create_staging_buffer_from_file(const std::string& texture_path)
        -> std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>;
    auto
    create_staging_buffer_from_rgba(std::vector<unsigned char>& rgba_image,
                                    const uint32_t width,
                                    const uint32_t height)
        -> std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>;
    void create_texture_image(const vk::raii::Buffer staging_buffer,
                              const vk::raii::DeviceMemory buffer_memory);
    void create_texture_image_view();
    void create_texture_sampler();
};
