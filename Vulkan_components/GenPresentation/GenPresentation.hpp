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

// This class creates all structure that need for presentation images on the
// screen. It's configured for simple 3D graphics.

#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <vector>

class GenPresentation {
public:
    GenPresentation(const vk::raii::Instance& instance, SDL_Window* window,
                    const vk::raii::PhysicalDevice& physical_device);

    void create_surface();
    [[nodiscard]] auto get_logical_device() const -> const vk::raii::Device&;
    [[nodiscard]] auto get_physical_device() const
        -> const vk::raii::PhysicalDevice&;
    [[nodiscard]] auto get_swapchain_surface_format() const
        -> const vk::SurfaceFormatKHR&;
    [[nodiscard]] auto get_queue_index() const -> uint32_t;
    [[nodiscard]] auto get_swapchain_images() -> std::vector<vk::Image>&;
    [[nodiscard]] auto get_swapchain_extent() -> vk::Extent2D&;
    [[nodiscard]] auto get_swapchain_image_views()
        -> std::vector<vk::raii::ImageView>&;
    [[nodiscard]] auto get_swapchain() -> vk::raii::SwapchainKHR&;
    [[nodiscard]] auto get_presentation_queue() -> vk::raii::Queue&;
    void recreate_swapchain();
    [[nodiscard]] auto get_depth_image_view() -> vk::raii::ImageView&;
    [[nodiscard]] auto get_depth_image() -> vk::raii::Image&;
    [[nodiscard]] auto get_depth_format() -> vk::Format&;
    [[nodiscard]] auto get_color_image() -> vk::raii::Image&;
    [[nodiscard]] auto get_color_image_view() -> vk::raii::ImageView&;
    [[nodiscard]] auto get_msaa_samples() -> vk::SampleCountFlagBits;
    [[nodiscard]] auto get_sdl_window() -> SDL_Window*;
    [[nodiscard]] auto get_instance() const -> const vk::raii::Instance&;

private:
    const vk::raii::Instance& instance;
    vk::raii::SurfaceKHR surface { nullptr };
    vk::raii::Device logical_device { nullptr };
    uint32_t queue_index = ~0;
    std::vector<const char*> required_device_extension
        = { vk::KHRSwapchainExtensionName };
    vk::raii::Queue presentation_queue { nullptr };

    vk::raii::SwapchainKHR swapchain = nullptr;
    std::vector<vk::Image> swapchain_images;
    vk::SurfaceFormatKHR swapchain_surface_format;
    vk::Extent2D swapchain_extent;
    std::vector<vk::raii::ImageView> swapchain_image_views;
    const vk::raii::PhysicalDevice& physical_device;

    SDL_Window* sdl_window;

    vk::raii::Image depth_image = nullptr;
    vk::raii::DeviceMemory depth_image_memory = nullptr;
    vk::raii::ImageView depth_image_view = nullptr;
    vk::Format depth_format;

    vk::SampleCountFlagBits msaa_samples = vk::SampleCountFlagBits::e1;

    vk::raii::Image color_image { nullptr };
    vk::raii::DeviceMemory color_image_memory { nullptr };
    vk::raii::ImageView color_image_view { nullptr };

    void create_logical_device();

    static auto choose_swap_surface_format(
        const std::vector<vk::SurfaceFormatKHR>& available_formats)
        -> vk::SurfaceFormatKHR;
    static auto choose_swap_present_mode(
        const std::vector<vk::PresentModeKHR>& available_present_modes)
        -> vk::PresentModeKHR;
    auto
    choose_swap_extent(const vk::SurfaceCapabilitiesKHR& surface_capabilities)
        -> vk::Extent2D;
    static auto choose_swap_min_image_count(
        const vk::SurfaceCapabilitiesKHR& surface_capabilities) -> uint32_t;
    void create_swapchain();
    void cleanup_swapchain();
    void create_image_views();

    void create_depth_resources();
    [[nodiscard]] auto
    find_supported_format(const std::vector<vk::Format>& candidates,
                          vk::ImageTiling tiling,
                          vk::FormatFeatureFlags features) -> vk::Format;
    [[nodiscard]] auto find_depth_format() -> vk::Format;
    [[nodiscard]] auto has_stencil_component(vk::Format format) -> bool;

    [[nodiscard]] auto get_max_usable_sample_count()
        -> vk::SampleCountFlagBits;

    void create_color_resources();
};
