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

// This class stores loaded to a mesh image into a container (unordered map).

#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <imgui.h>

#include <unordered_map>
#include <vector>

class ImGuiTextureManager {
private:
    const vk::raii::Device& logical_device { nullptr };
    const vk::raii::DescriptorPool& descriptor_pool { nullptr };
    vk::raii::DescriptorSetLayout descriptor_set_layout { nullptr };

    struct TextureInfo {
        vk::raii::Image image { nullptr };
        vk::raii::DeviceMemory image_memory { nullptr };
        vk::raii::ImageView image_view { nullptr };
        vk::raii::Sampler sampler { nullptr };
        vk::raii::DescriptorSet descriptor_set { nullptr };
        uint32_t width { 0 };
        uint32_t height { 0 };
    };

    std::unordered_map<std::string, TextureInfo> textures;
    std::vector<TextureInfo> plot_textures;

    void create_descriptor_set_layout();

public:
    ImGuiTextureManager(const vk::raii::Device& logical_device,
                        const vk::raii::DescriptorPool& descriptor_pool);

    // Register a texture for use with ImGui
    void register_texture(vk::raii::Image image,
                          vk::raii::DeviceMemory image_memory,
                          vk::raii::ImageView image_view,
                          vk::raii::Sampler sampler, uint32_t width,
                          uint32_t height, const std::string& name = "");

    // Get a previously registered texture
    [[nodiscard]] auto get_texture(const std::string& name) -> ImTextureID;
    [[nodiscard]] auto get_plot_textures() const
        -> const std::vector<TextureInfo>&;
};
