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

#include "Vudiplot.hpp"

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS 1
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <GenInstance.hpp>
#include <GenPresentation.hpp>
#include <GenCommandBuffer.hpp>
#include <ImGuiVulkanUtil.hpp>
#include <GenSDLWindow.hpp>
#include <InitUtils.hpp>
#include <GenGettext.hpp>

#include <cstdlib>
#include <iostream>
#include <libintl.h>
#include <locale.h>
#include <filesystem>

template <std::derived_from<VudiInit::PlotInit> T>
class Vudiplot<T>::VudiImpl {
public:
    VudiImpl()
    {
        init();
    }

    ~VudiImpl()
    {
    }

    VudiImpl(const VudiImpl& src) = delete;
    VudiImpl& operator=(const VudiImpl& src) = delete;
    VudiImpl(VudiImpl&& rhs) = delete;
    VudiImpl& operator=(VudiImpl&& rhs) = delete;

    void init()
    {
        const std::string resources_dir { RESOURCES_DIR };
        auto gen_gettext { std::make_unique<GenGettext>(resources_dir) };
        if (!gen_gettext->init()) {
            throw std::runtime_error(gettext("Gettext init failed!"));
        }
        gen_sdl_window = std::make_unique<GenSDLWindow>();
        gen_sdl_window->init_sdl_window(window_name, width, height);

        const std::string app_name = "Vudiplot API";
        // Create Vulkan instance
        gen_instance = std::make_unique<GenInstance>(app_name);
        const auto& instance { gen_instance->get_instance() };
        const auto& cur_physical_device {
            gen_instance->get_physical_device()
        };

        // Create Vulkan presentation layer
        gen_presentation = std::make_unique<GenPresentation>(
            instance, gen_sdl_window->get_sdl_window(), cur_physical_device);

        const auto& logical_device { gen_presentation->get_logical_device() };

        const uint32_t queue_index = gen_presentation->get_queue_index();

        // Create Vulkan command buffer for recording rendering commands
        gen_command_buffer = std::make_unique<GenCommandBuffer>(
            queue_index, logical_device, max_frame_in_flight);

        const std::string spv_shader_path { resources_dir
                                            + "/shaders/imgui.spv" };

        auto imgui_shader_info { std::make_unique<ShaderInitInfo>(
            ShaderInitInfo {
                .spv_shader_path { spv_shader_path },
                .vert_func_name { "vert_main" },
                .frag_func_name { "frag_main" },

            }) };

        const std::string& texture_path { resources_dir
                                          + "/textures/KTX_images" };
        // Create a class that represents Dear ImGui
        imgui_util = std::make_unique<ImGuiVulkanUtil>(
            gen_presentation, imgui_shader_info, max_frame_in_flight,
            texture_path);

        const std::string font_path {
            resources_dir + "/fonts/GeistMonoNerdFontMono-Regular.otf"
        };

        // Initialize Dear ImGui and its resources
        auto is_imgui_init { imgui_util->init_ImGui(width, height,
                                                    font_path) };
        if (!is_imgui_init) {
            throw std::runtime_error(gettext("ImGui is not initialized!"));
        }
        if (!imgui_util->init_resources()) {
            throw std::runtime_error(gettext("Resources is not initialized!"));
        }
    }

    auto get_imgui_util() -> std::unique_ptr<ImGuiVulkanUtil>&
    {
        return imgui_util;
    }

    auto get_gen_presentation() -> std::unique_ptr<GenPresentation>&
    {
        return gen_presentation;
    }

    auto get_gen_command_buffer() -> std::unique_ptr<GenCommandBuffer>&
    {
        return gen_command_buffer;
    }

    auto get_sdl_window() -> SDL_Window*
    {
        return gen_sdl_window->get_sdl_window();
    }

    auto get_plot_init() -> std::unique_ptr<T>&
    {
        return plot_init;
    }

    void clean_up_resources()
    {
        if (imgui_util) {
            imgui_util.reset();
        }
        if (gen_command_buffer) {
            gen_command_buffer.reset();
        }
        if (gen_presentation) {
            gen_presentation.reset();
        }
        if (gen_instance) {
            gen_instance.reset();
        }
        if (gen_sdl_window) {
            gen_sdl_window.reset();
        }
    }

private:
    const uint32_t height { 600 };
    const uint32_t width { 800 };
    const std::string window_name { "Vudiplot" };

    std::unique_ptr<GenSDLWindow> gen_sdl_window { nullptr };
    std::unique_ptr<GenInstance> gen_instance;
    std::unique_ptr<GenPresentation> gen_presentation;
    const uint32_t max_frame_in_flight { 2 };
    std::unique_ptr<GenCommandBuffer> gen_command_buffer;
    std::unique_ptr<ImGuiVulkanUtil> imgui_util;

    std::unique_ptr<T> plot_init { nullptr };
};

template <std::derived_from<VudiInit::PlotInit> T>
Vudiplot<T>::Vudiplot()
    : vudi_implt(std::make_unique<VudiImpl>())
{
}

template <std::derived_from<VudiInit::PlotInit> T>
Vudiplot<T>::~Vudiplot() = default;

template <std::derived_from<VudiInit::PlotInit> T>
Vudiplot<T>::Vudiplot(Vudiplot<T>&& rhs) noexcept = default;

template <std::derived_from<VudiInit::PlotInit> T>
Vudiplot<T>& Vudiplot<T>::operator=(Vudiplot<T>&& rhs) noexcept = default;

template <std::derived_from<VudiInit::PlotInit> T> void Vudiplot<T>::show()
{
    uint32_t frame_index { 0 };
    bool is_running { true };
    bool framebuffer_resized { false };

    while (is_running) {
        // Check window size and if it's minimized prevent its rendering
        int width, height;
        SDL_GetWindowSizeInPixels(vudi_implt->get_sdl_window(), &width,
                                  &height);

        if (width == 0 || height == 0) {
            SDL_Delay(16);
            continue; // minimized — don't render
        }

        SDL_Event event;

        // Check if some keys are pressed or mouse is clicked
        vudi_implt->get_imgui_util()->handle_key_press(event, is_running,
                                                       framebuffer_resized);

        if (!is_running) {
            break;
        }

        // Call to created GUI, must be only one at time
        // Simple line plot created with ImPlot
        if (!vudi_implt->get_imgui_util()->plot_frame(
                vudi_implt->get_plot_init(), is_running)) {
            std::cout << "Plot frame failed!\n";
            throw std::runtime_error(gettext("Plot frame failed!"));
        }

        // Draw selected GUI or plot
        if (!vudi_implt->get_imgui_util()->draw_frame( //
                vudi_implt->get_gen_command_buffer()->get_command_buffers(),
                frame_index, framebuffer_resized)) {
            throw std::runtime_error("Draw frame failed!");
        }
    }
    if (vudi_implt->get_gen_presentation()) {
        vudi_implt->get_gen_presentation()->get_logical_device().waitIdle();
    }
}

template <std::derived_from<VudiInit::PlotInit> T>
auto Vudiplot<T>::set_plot_params(const T& plot_config) -> bool
{
    if constexpr (std::is_same_v<T, VudiInit::ImagePlot>) {
        auto raw_images { plot_config.raw_images };
        auto images_width { plot_config.images_width };
        auto images_height { plot_config.images_height };
        if (!vudi_implt->get_imgui_util()->load_plot_images(
                raw_images, images_width, images_height)) {
            std::cerr << gettext("Plot images didn't downloaded!") << "\n";
            return false;
        }
    }
    vudi_implt->get_plot_init() = std::make_unique<T>(plot_config);
    if (vudi_implt->get_plot_init() == nullptr) {
        std::cerr << gettext("Wrong plot_config. plot_init is nullptr!")
                  << "\n";
        return false;
    }
    return true;
}

template <std::derived_from<VudiInit::PlotInit> T>
void Vudiplot<T>::clean_up_resources()
{
    vudi_implt->clean_up_resources();
}

template class Vudiplot<VudiInit::LinePlot>;
template class Vudiplot<VudiInit::HeatmapsPlot>;
template class Vudiplot<VudiInit::DemoPlot>;
template class Vudiplot<VudiInit::ImagePlot>;
