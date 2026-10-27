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

// This class creates Vulkan's context, instance and physical device

#pragma once
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <map>

class GenInstance {

public:
    explicit GenInstance(const std::string& app_name,
                         const std::string& engine_name = "No engine");
    [[nodiscard]] auto get_instance() -> vk::raii::Instance&;
    [[nodiscard]] auto get_physical_device() -> vk::raii::PhysicalDevice&;

private:
    vk::raii::Context context;
    vk::raii::Instance instance { nullptr };
    std::string app_name;
    std::string engine_name;
    const std::vector<char const*> validation_layers
        = { "VK_LAYER_KHRONOS_validation" };
    static constexpr bool enable_validation_layers { false };
    vk::raii::DebugUtilsMessengerEXT debug_messenger { nullptr };
    vk::raii::PhysicalDevice cur_physical_device { nullptr };
    std::vector<const char*> required_device_extensions
        = { vk::KHRSwapchainExtensionName };

    void create_instance();
    auto get_required_extensions() -> std::vector<const char*>;
    void setup_debug_messenger();
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
        vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT* p_callback_data, void*);
    void pick_physical_device();
    auto
    get_selected_physical_devices(const vk::raii::PhysicalDevices& devices)
        const -> std::multimap<int, vk::raii::PhysicalDevice>;
    auto is_device_suitable(const vk::raii::PhysicalDevice& physical_device)
        -> bool;
};
