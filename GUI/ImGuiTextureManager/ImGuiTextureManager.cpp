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

#include "ImGuiTextureManager.hpp"
#include <iostream>

ImGuiTextureManager::ImGuiTextureManager(
    const vk::raii::Device& logical_device,
    const vk::raii::DescriptorPool& descriptor_pool)
    : logical_device(logical_device)
    , descriptor_pool(descriptor_pool)
{
    create_descriptor_set_layout();
}

void ImGuiTextureManager::create_descriptor_set_layout()
{
    // Create descriptor set layout for textures
    vk::DescriptorSetLayoutBinding binding {
        .binding = 0,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
    };

    vk::DescriptorSetLayoutCreateInfo layout_info { .bindingCount = 1,
                                                    .pBindings = &binding };

    descriptor_set_layout
        = vk::raii::DescriptorSetLayout(logical_device, layout_info);
}

void ImGuiTextureManager::register_texture(vk::raii::Image image,
                                           vk::raii::DeviceMemory image_memory,
                                           vk::raii::ImageView image_view,
                                           vk::raii::Sampler sampler,
                                           uint32_t width, uint32_t height,
                                           const std::string& name)
{
    /* This function creates and saves image descriptor into an unordered map.
           Image, image memory, image view and sampler also must be saved
           for the descriptor to be valid and usable.
           The unordered map has string as key (filename without extension)
       that will be used to get apropriate descriptor during the GUI frame
       creation. The descriptor will be casted to ImTextureID that will be a
       key in another unordered map that responsible for access to proper
           texture images for drawing at the screen.
     */

    // Allocate descriptor set
    std::vector<vk::DescriptorSetLayout> layouts(1, descriptor_set_layout);

    vk::DescriptorSetAllocateInfo alloc_info {
        .descriptorPool = descriptor_pool,
        // Source pool for allocation
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        // Number of sets to allocate
        .pSetLayouts = layouts.data()
    };

    vk::raii::DescriptorSet descriptor_set
        = std::move(logical_device.allocateDescriptorSets(alloc_info).front());

    // Update descriptor set
    vk::DescriptorImageInfo image_info {
        .sampler = sampler,
        .imageView = image_view,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };

    vk::WriteDescriptorSet write_set {
        .dstSet = descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .pImageInfo = &image_info,

    };
    logical_device.updateDescriptorSets(write_set, {});

    TextureInfo tex_info { .image = std::move(image),
                           .image_memory = std::move(image_memory),
                           .image_view = std::move(image_view),
                           .sampler = std::move(sampler),
                           .descriptor_set = std::move(descriptor_set),
                           .width = width,
                           .height = height };

    // Store texture info
    if (!name.empty()) {
        textures[name] = std::move(tex_info);
    } else {
        plot_textures.emplace_back(std::move(tex_info));
    }
}

auto ImGuiTextureManager::get_texture(const std::string& name) -> ImTextureID
{
    if (textures.find(name) == textures.end()) {
        std::cerr << gettext("Texture not found: ") + name << "\n";
        return ImTextureID_Invalid;
    }
    return (ImTextureID)(VkDescriptorSet)*textures[name].descriptor_set;
}

auto ImGuiTextureManager::get_plot_textures() const
    -> const std::vector<TextureInfo>&
{
    return plot_textures;
}
