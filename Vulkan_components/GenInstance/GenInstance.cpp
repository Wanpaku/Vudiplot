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

#include "GenInstance.hpp"

#include <vector>
#include <string>
#include <iostream>

GenInstance::GenInstance(const std::string& app_name,
                         const std::string& engine_name)
    : app_name(app_name)
    , engine_name(engine_name)

{
    create_instance();
    setup_debug_messenger();
    pick_physical_device();
}

auto GenInstance::get_required_extensions() -> std::vector<const char*>
{
    // Отримання переліку необхідних розширень

    Uint32 sdl_extension_count = 0;

    const char* const* sdl_extensions
        = SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count);

    if (!sdl_extensions) {
        SDL_Log(gettext("SDL_Vulkan_GetInstanceExtensions failed: %s"),
                SDL_GetError());
        throw std::runtime_error(gettext("SDL extensions failed."));
    }

    std::vector<const char*> extensions(sdl_extensions,
                                        sdl_extensions + sdl_extension_count);

    if (enable_validation_layers) {
        extensions.emplace_back(vk::EXTDebugUtilsExtensionName);
    }

    return extensions;
}

void GenInstance::create_instance()
{
    // Створення екземпляру Vulkan
    const vk::ApplicationInfo appInfo { .pApplicationName = app_name.c_str(),
                                        .applicationVersion
                                        = VK_MAKE_VERSION(1, 0, 0),
                                        .pEngineName = engine_name.c_str(),
                                        .engineVersion
                                        = VK_MAKE_VERSION(1, 0, 0),
                                        .apiVersion = vk::ApiVersion13 };

    std::vector<char const*> required_layers;
    if (enable_validation_layers) {
        required_layers.assign(validation_layers.begin(),
                               validation_layers.end());
    }

    auto layer_props = context.enumerateInstanceLayerProperties();
    auto unsupported_layer_it = std::ranges::find_if(
        required_layers, [&layer_props](const auto& required_layer) {
            return std::ranges::none_of(
                layer_props, [required_layer](const auto& layer_prop) {
                    return strcmp(layer_prop.layerName, required_layer) == 0;
                });
        });

    if (unsupported_layer_it != required_layers.end()) {
        throw std::runtime_error(gettext("Required layer not supported: ")
                                 + std::string(*unsupported_layer_it));
    }

    auto required_extensions = get_required_extensions();

    auto extension_props = context.enumerateInstanceExtensionProperties();
    auto unsupported_extension_it = std::ranges::find_if(
        required_extensions,
        [&extension_props](auto const& required_extension) {
            return std::ranges::none_of(
                extension_props,
                [required_extension](const auto& extension_prop) {
                    return strcmp(extension_prop.extensionName,
                                  required_extension)
                        == 0;
                });
        });

    if (unsupported_extension_it != required_extensions.end()) {
        throw std::runtime_error(gettext("Required extension not supported: ")
                                 + std::string(*unsupported_extension_it));
    }

    vk::InstanceCreateInfo createInfo {
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(required_layers.size()),
        .ppEnabledLayerNames = required_layers.data(),
        .enabledExtensionCount
        = static_cast<uint32_t>(required_extensions.size()),
        .ppEnabledExtensionNames = required_extensions.data()
    };
    instance = vk::raii::Instance(context, createInfo);
}

[[nodiscard]] auto GenInstance::get_instance() -> vk::raii::Instance&
{
    // Отримання створенного екземпляру Vulkan
    return instance;
}

void GenInstance::setup_debug_messenger()
{
    // Налаштування повідомлень дебагера
    if (!enable_validation_layers) {
        return;
    }

    vk::DebugUtilsMessageSeverityFlagsEXT severity_flags(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
        | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

    vk::DebugUtilsMessageTypeFlagsEXT message_type_flags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
        | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
        | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

    vk::DebugUtilsMessengerCreateInfoEXT
        debug_utils_messenger_create_info_EXT {
            .messageSeverity = severity_flags,
            .messageType = message_type_flags,
            .pfnUserCallback = &debug_callback
        };
    debug_messenger = instance.createDebugUtilsMessengerEXT(
        debug_utils_messenger_create_info_EXT);
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL GenInstance::debug_callback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    vk::DebugUtilsMessageTypeFlagsEXT type,
    const vk::DebugUtilsMessengerCallbackDataEXT* p_callback_data, void*)
{

    // Налаштування виведення повідомлень дебагера
    if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
        || severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
        std::cerr << gettext("Validation layer: type ") << to_string(type)
                  << gettext(" message: ") << p_callback_data->pMessage
                  << "\n";
    }

    return vk::False;
}

auto GenInstance::get_selected_physical_devices(
    const vk::raii::PhysicalDevices& devices) const
    -> std::multimap<int, vk::raii::PhysicalDevice>
{
    // Вибір фізичних пристроїв згідно заданих параметрів швидкодії та
    // створення їхнього рейтингу за допомогою multimap
    std::multimap<int, vk::raii::PhysicalDevice> candidates;

    for (const auto& device : devices) {
        auto device_props { device.getProperties() };
        uint32_t score { 0 };

        if (device_props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            score += 1000;
        }

        score += device_props.limits.maxImageDimension2D;
        candidates.emplace(score, device);
    }

    return candidates;
}

auto GenInstance::is_device_suitable(
    const vk::raii::PhysicalDevice& physical_device) -> bool
{
    // Перевірка чи відповідає фізичний пристрій заданим параметрам програми

    // Check if the physicalDevice supports the Vulkan 1.4 API version
    bool supports_vulkan1_4
        = physical_device.getProperties().apiVersion >= VK_API_VERSION_1_3;

    // Check if any of the queue families support graphics operations
    auto queue_families = physical_device.getQueueFamilyProperties();
    bool supports_graphics
        = std::ranges::any_of(queue_families, [](const auto& qfp) {
              return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
          });

    // Check if all required physicalDevice extensions are available
    auto available_device_extensions
        = physical_device.enumerateDeviceExtensionProperties();
    bool supports_all_required_extensions = std::ranges::all_of(
        required_device_extensions,
        [&available_device_extensions](const auto& required_device_extension) {
            return std::ranges::any_of(
                available_device_extensions,
                [required_device_extension](
                    const auto& available_device_extension) {
                    return strcmp(available_device_extension.extensionName,
                                  required_device_extension)
                        == 0;
                });
        });

    // Check if the physicalDevice supports the required features
    auto features = physical_device.template getFeatures2<
        vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
    bool supports_required_features
        = features.template get<vk::PhysicalDeviceFeatures2>()
              .features.samplerAnisotropy
        && features.template get<vk::PhysicalDeviceVulkan11Features>()
               .shaderDrawParameters
        && features.template get<vk::PhysicalDeviceVulkan13Features>()
               .dynamicRendering
        && features.template get<vk::PhysicalDeviceVulkan13Features>()
               .synchronization2
        && features
               .template get<
                   vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
               .extendedDynamicState;

    // Return true if the physicalDevice meets all the criteria
    return supports_vulkan1_4 && supports_graphics
        && supports_all_required_extensions && supports_required_features;
}

void GenInstance::pick_physical_device()
{
    // Вибір поточного фізичного пристрою
    const auto physical_devices { vk::raii::PhysicalDevices(instance) };
    auto candidates { get_selected_physical_devices(physical_devices) };

    if (!candidates.empty() && candidates.rbegin()->first > 0) {
        auto dev_iter { std::ranges::find_if(
            candidates.rbegin(), candidates.rend(),
            [&](const auto& physical_device) {
                return is_device_suitable(physical_device.second);
            }) };
        if (dev_iter == candidates.rend()) {
            throw std::runtime_error(
                gettext("Failed to find a suitable GPU!"));
        }
        cur_physical_device = dev_iter->second;

    } else {
        throw std::runtime_error(
            gettext("Failed to find GPUs with Vulkan support!"));
    }
}

auto GenInstance::get_physical_device() -> vk::raii::PhysicalDevice&
{
    return cur_physical_device;
}
