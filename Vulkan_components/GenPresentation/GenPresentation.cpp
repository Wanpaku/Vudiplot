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

#include "GenPresentation.hpp"
#include <BufferUtils.hpp>
#include <iostream>

GenPresentation::GenPresentation(
    const vk::raii::Instance& instance, SDL_Window* window,
    const vk::raii::PhysicalDevice& physical_device)
    : instance(instance)
    , physical_device(physical_device)
    , sdl_window(window)
{
    msaa_samples = get_max_usable_sample_count();
    create_surface();
    create_logical_device();
    create_swapchain();
    create_image_views();
    create_color_resources();
    create_depth_resources();
}

void GenPresentation::create_surface()
{
    VkSurfaceKHR _surface;

    if (!SDL_Vulkan_CreateSurface(sdl_window, *instance,
                                  nullptr, // default Vulkan allocator
                                  &_surface)) {
        SDL_Log(gettext("SDL_Vulkan_CreateSurface failed: %s\n"),
                SDL_GetError());

        SDL_DestroyWindow(sdl_window);
        SDL_Quit();

        throw std::runtime_error(gettext("Failed to create Vulkan surface"));
    }

    surface = vk::raii::SurfaceKHR(instance, _surface);
}

void GenPresentation::create_logical_device()
{
    std::vector<vk::QueueFamilyProperties> queue_family_properties
        = physical_device.getQueueFamilyProperties();

    // get the first index into queueFamilyProperties which supports both
    // graphics and present
    for (uint32_t qfp_index = 0; qfp_index < queue_family_properties.size();
         qfp_index++) {
        if ((queue_family_properties[qfp_index].queueFlags
             & vk::QueueFlagBits::eGraphics)
            && physical_device.getSurfaceSupportKHR(qfp_index, *surface)) {
            // found a queue family that supports both graphics and
            // presentation
            queue_index = qfp_index;
            break;
        }
    }
    if (queue_index == ~0) {
        throw std::runtime_error(
            gettext("Could not find a queue for graphics and "
                    "presentation -> terminating"));
    }
    // create a Device
    float queue_priority = 0.5f;
    vk::DeviceQueueCreateInfo device_queue_create_info { .queueFamilyIndex
                                                         = queue_index,
                                                         .queueCount = 1,
                                                         .pQueuePriorities
                                                         = &queue_priority };

    //    query for Vulkan 1.3 features
    vk::StructureChain<vk::PhysicalDeviceFeatures2,
                       vk::PhysicalDeviceVulkan11Features,
                       vk::PhysicalDeviceVulkan13Features,
                       vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
        feature_chain = {
            { .features
              = { .samplerAnisotropy = true } }, // vk::PhysicalDeviceFeatures2
            { .shaderDrawParameters
              = true }, // vk::PhysicalDeviceVulkan11Features
            { .synchronization2 = true,
             .dynamicRendering = true }, // vk::PhysicalDeviceVulkan13Features
            { .extendedDynamicState
              = true }  // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
    };

    vk::DeviceCreateInfo device_create_info {
        .pNext = &feature_chain.get<vk::PhysicalDeviceFeatures2>(),
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &device_queue_create_info,
        .enabledExtensionCount
        = static_cast<uint32_t>(required_device_extension.size()),
        .ppEnabledExtensionNames = required_device_extension.data()
    };
    logical_device = vk::raii::Device(physical_device, device_create_info);
    presentation_queue = vk::raii::Queue(logical_device, queue_index, 0);
}

auto GenPresentation::choose_swap_surface_format(
    const std::vector<vk::SurfaceFormatKHR>& available_formats)
    -> vk::SurfaceFormatKHR
{
    assert(!available_formats.empty());
    const auto format_it
        = std::ranges::find_if(available_formats, [](const auto& format) {
              return format.format == vk::Format::eB8G8R8A8Srgb
                  && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
          });
    return format_it != available_formats.end() ? *format_it
                                                : available_formats[0];
}

auto GenPresentation::choose_swap_present_mode(
    const std::vector<vk::PresentModeKHR>& available_present_modes)
    -> vk::PresentModeKHR
{
    assert(std::ranges::any_of(available_present_modes, [](auto present_mode) {
        return present_mode == vk::PresentModeKHR::eFifo;
    }));
    return std::ranges::any_of(available_present_modes,
                               [](const vk::PresentModeKHR value) {
                                   return vk::PresentModeKHR::eMailbox
                                       == value;
                               })
        ? vk::PresentModeKHR::eMailbox
        : vk::PresentModeKHR::eFifo;
}

auto GenPresentation::choose_swap_extent(
    const vk::SurfaceCapabilitiesKHR& surface_capabilities) -> vk::Extent2D
{
    if (surface_capabilities.currentExtent.width
        != std::numeric_limits<uint32_t>::max()) {
        return surface_capabilities.currentExtent;
    }

    int width, height;

    if (!SDL_GetWindowSizeInPixels(sdl_window, &width, &height)) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     gettext("SDL_GetWindowSizeInPixels failed: %s"),
                     SDL_GetError());
        throw std::runtime_error(gettext("SDL get window size failed."));
    }

    return { std::clamp<uint32_t>(width,
                                  surface_capabilities.minImageExtent.width,
                                  surface_capabilities.maxImageExtent.width),
             std::clamp<uint32_t>(
                 height, surface_capabilities.minImageExtent.height,
                 surface_capabilities.maxImageExtent.height) };
}

auto GenPresentation::choose_swap_min_image_count(
    const vk::SurfaceCapabilitiesKHR& surface_capabilities) -> uint32_t
{
    auto min_image_count = std::max(3u, surface_capabilities.minImageCount);
    if ((0 < surface_capabilities.maxImageCount)
        && (surface_capabilities.maxImageCount < min_image_count)) {
        min_image_count = surface_capabilities.maxImageCount;
    }
    return min_image_count;
}

void GenPresentation::create_swapchain()
{
    vk::SurfaceCapabilitiesKHR surface_capabilities {
        physical_device.getSurfaceCapabilitiesKHR(*surface)
    };

    swapchain_extent = choose_swap_extent(surface_capabilities);
    uint32_t min_image_count { choose_swap_min_image_count(
        surface_capabilities) };

    std::vector<vk::SurfaceFormatKHR> available_formats
        = physical_device.getSurfaceFormatsKHR(*surface);
    swapchain_surface_format = choose_swap_surface_format(available_formats);

    std::vector<vk::PresentModeKHR> available_present_modes {
        physical_device.getSurfacePresentModesKHR(*surface)
    };
    vk::PresentModeKHR present_mode { choose_swap_present_mode(
        available_present_modes) };

    vk::SwapchainCreateInfoKHR swapchain_create_info {
        .surface = *surface,
        .minImageCount = min_image_count,
        .imageFormat = swapchain_surface_format.format,
        .imageColorSpace = swapchain_surface_format.colorSpace,
        .imageExtent = swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment
            | vk::ImageUsageFlagBits::eTransferSrc,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = present_mode,
        .clipped = true
    };

    swapchain = vk::raii::SwapchainKHR(logical_device, swapchain_create_info);
    swapchain_images = swapchain.getImages();
}

void GenPresentation::create_image_views()
{
    assert(swapchain_image_views.empty());

    vk::ImageViewCreateInfo image_view_create_info {
        .viewType = vk::ImageViewType::e2D,
        .format = swapchain_surface_format.format,
        .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
    };
    for (const auto& image : swapchain_images) {
        image_view_create_info.image = image;
        swapchain_image_views.emplace_back(logical_device,
                                           image_view_create_info);
    }
}

auto GenPresentation::get_logical_device() const -> const vk::raii::Device&
{
    return logical_device;
}

auto GenPresentation::get_swapchain_surface_format() const
    -> const vk::SurfaceFormatKHR&
{
    return swapchain_surface_format;
}

auto GenPresentation::get_queue_index() const -> uint32_t
{
    return queue_index;
}

auto GenPresentation::get_swapchain_images() -> std::vector<vk::Image>&
{
    return swapchain_images;
}

auto GenPresentation::get_swapchain_extent() -> vk::Extent2D&
{
    return swapchain_extent;
}

auto GenPresentation::get_swapchain_image_views()
    -> std::vector<vk::raii::ImageView>&
{
    return swapchain_image_views;
}

auto GenPresentation::get_swapchain() -> vk::raii::SwapchainKHR&
{
    return swapchain;
}

auto GenPresentation::get_presentation_queue() -> vk::raii::Queue&
{
    return presentation_queue;
}

void GenPresentation::cleanup_swapchain()
{
    swapchain_image_views.clear();
    swapchain = nullptr;
}

void GenPresentation::recreate_swapchain()
{
    logical_device.waitIdle();

    cleanup_swapchain();
    create_swapchain();
    create_image_views();
    create_color_resources();
    create_depth_resources();
}

auto GenPresentation::find_supported_format(
    const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
    vk::FormatFeatureFlags features) -> vk::Format
{
    auto format_it = std::ranges::find_if(candidates, [&](auto const format) {
        vk::FormatProperties props
            = physical_device.getFormatProperties(format);
        return (
            ((tiling == vk::ImageTiling::eLinear)
             && ((props.linearTilingFeatures & features) == features))
            || ((tiling == vk::ImageTiling::eOptimal)
                && ((props.optimalTilingFeatures & features) == features)));
    });
    if (format_it == candidates.end()) {
        throw std::runtime_error(
            gettext("Failed to find supported format for presentation!"));
    }
    return *format_it;
}

auto GenPresentation::find_depth_format() -> vk::Format
{
    return find_supported_format(
        { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
          vk::Format::eD24UnormS8Uint },
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

auto GenPresentation::has_stencil_component(vk::Format format) -> bool
{
    return format == vk::Format::eD32SfloatS8Uint
        || format == vk::Format::eD24UnormS8Uint;
}

void GenPresentation::create_depth_resources()
{
    const auto memory_properties = physical_device.getMemoryProperties();

    depth_format = find_depth_format();
    auto buffer_utils { std::make_unique<BufferUtils>(
        memory_properties, logical_device, queue_index) };

    std::tie(depth_image, depth_image_memory) = buffer_utils->create_image(
        swapchain_extent.width, swapchain_extent.height, depth_format,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal, 1, msaa_samples);
    depth_image_view = buffer_utils->create_imageview(
        depth_image, depth_format, vk::ImageAspectFlagBits::eDepth, 1);
}

auto GenPresentation::get_depth_image_view() -> vk::raii::ImageView&
{
    return depth_image_view;
}

auto GenPresentation::get_depth_image() -> vk::raii::Image&
{
    return depth_image;
}

auto GenPresentation::get_depth_format() -> vk::Format&
{
    return depth_format;
}

auto GenPresentation::get_max_usable_sample_count() -> vk::SampleCountFlagBits
{
    vk::PhysicalDeviceProperties physicalDeviceProperties
        = physical_device.getProperties();

    vk::SampleCountFlags counts
        = physicalDeviceProperties.limits.framebufferColorSampleCounts
        & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & vk::SampleCountFlagBits::e64) {
        return vk::SampleCountFlagBits::e64;
    }
    if (counts & vk::SampleCountFlagBits::e32) {
        return vk::SampleCountFlagBits::e32;
    }
    if (counts & vk::SampleCountFlagBits::e16) {
        return vk::SampleCountFlagBits::e16;
    }
    if (counts & vk::SampleCountFlagBits::e8) {
        return vk::SampleCountFlagBits::e8;
    }
    if (counts & vk::SampleCountFlagBits::e4) {
        return vk::SampleCountFlagBits::e4;
    }
    if (counts & vk::SampleCountFlagBits::e2) {
        return vk::SampleCountFlagBits::e2;
    }

    return vk::SampleCountFlagBits::e1;
}

void GenPresentation::create_color_resources()
{
    const auto memory_properties = physical_device.getMemoryProperties();

    auto buffer_utils { std::make_unique<BufferUtils>(
        memory_properties, logical_device, queue_index) };

    std::tie(color_image, color_image_memory) = buffer_utils->create_image(
        swapchain_extent.width, swapchain_extent.height,
        swapchain_surface_format.format, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransientAttachment
            | vk::ImageUsageFlagBits::eColorAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal, 1, msaa_samples);
    color_image_view = buffer_utils->create_imageview(
        color_image, swapchain_surface_format.format,
        vk::ImageAspectFlagBits::eColor, 1);
}

auto GenPresentation::get_color_image() -> vk::raii::Image&
{
    return color_image;
}

auto GenPresentation::get_color_image_view() -> vk::raii::ImageView&
{
    return color_image_view;
}

auto GenPresentation::get_msaa_samples() -> vk::SampleCountFlagBits
{
    return msaa_samples;
}

auto GenPresentation::get_physical_device() const
    -> const vk::raii::PhysicalDevice&
{
    return physical_device;
}

auto GenPresentation::get_sdl_window() -> SDL_Window*
{
    return sdl_window;
}

auto GenPresentation::get_instance() const -> const vk::raii::Instance&
{
    return instance;
}
