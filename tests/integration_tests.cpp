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

#include <catch2/catch_test_macros.hpp>

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <Vudiplot.hpp>
#include <lodepng.h>

#include <random>
#include <vector>
#include <thread>
#include <chrono>
#include <future>
#include <iostream>
#include <filesystem>
#include <numeric>
#include <dlfcn.h>

static auto generate_random_data(const size_t count) -> std::vector<double>
{
    // Generate random data for the line plot
    std::vector<double> y_arr;
    y_arr.reserve(count);
    std::random_device rand_dev;

    std::mt19937 rand_eng(rand_dev());
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    for (size_t i = 0; i < count; ++i) {
        auto number { dist(rand_eng) };
        y_arr.emplace_back(number);
    }

    return y_arr;
}

static void push_key_event(Uint32 type, SDL_WindowID window_id,
                           SDL_Scancode key_code, SDL_Keymod key_mod)
{
    SDL_Event event {};
    event.type = type;
    event.key.windowID = window_id;
    event.key.scancode = key_code;
    event.key.mod = key_mod;
    event.key.key = SDL_GetKeyFromScancode(key_code, key_mod, true);
    event.key.repeat = false;
    event.key.down = (type == SDL_EVENT_KEY_DOWN);
    SDL_PushEvent(&event);
}

static void simulate_key_combo(SDL_Scancode main_key, SDL_Keymod modifiers,
                               SDL_WindowID window_id)
{
    // Press modifier keys first
    if (modifiers & SDL_KMOD_CTRL) {
        push_key_event(SDL_EVENT_KEY_DOWN, window_id, SDL_SCANCODE_LCTRL,
                       modifiers);
    }
    if (modifiers & SDL_KMOD_SHIFT) {
        push_key_event(SDL_EVENT_KEY_DOWN, window_id, SDL_SCANCODE_LSHIFT,
                       modifiers);
    }
    if (modifiers & SDL_KMOD_ALT) {
        push_key_event(SDL_EVENT_KEY_DOWN, window_id, SDL_SCANCODE_LALT,
                       modifiers);
    }

    // Press main key
    push_key_event(SDL_EVENT_KEY_DOWN, window_id, main_key, modifiers);

    // Release main key
    push_key_event(SDL_EVENT_KEY_UP, window_id, main_key, modifiers);

    // Release modifier keys in reverse order
    if (modifiers & SDL_KMOD_ALT) {
        push_key_event(SDL_EVENT_KEY_UP, window_id, SDL_SCANCODE_LALT,
                       modifiers & ~SDL_KMOD_ALT);
    }
    if (modifiers & SDL_KMOD_SHIFT) {
        push_key_event(SDL_EVENT_KEY_UP, window_id, SDL_SCANCODE_LSHIFT,
                       modifiers & ~SDL_KMOD_SHIFT);
    }
    if (modifiers & SDL_KMOD_CTRL) {
        push_key_event(SDL_EVENT_KEY_UP, window_id, SDL_SCANCODE_LCTRL,
                       modifiers & ~SDL_KMOD_CTRL);
    }
}

static auto get_app_window() -> SDL_Window*
{
    int count = 0;
    SDL_Window* const* windows = SDL_GetWindows(&count);

    if (!windows || count == 0) {
        return nullptr;
    }

    return windows[0];
}

auto thread_draw_test(unsigned int period) -> bool
{
    SDL_Window* sdl_window { get_app_window() };
    REQUIRE(sdl_window != nullptr);
    bool result { false };
    bool run { true };
    auto start { std::chrono::steady_clock::now() };
    while (run) {
        const ImDrawData* draw_data = ImGui::GetDrawData();

        if (draw_data != nullptr && draw_data->Valid
            && (draw_data->CmdListsCount > 0)
            && (draw_data->TotalVtxCount > 0)) {
            run = false;
            result = true;
        } else {
            auto end { std::chrono::steady_clock::now() };
            if (std::chrono::duration_cast<std::chrono::seconds>(end - start)
                    .count()
                >= period) {
                run = false;
            }
        }
    }

    SDL_WindowID window_id { SDL_GetWindowID(sdl_window) };
    simulate_key_combo(SDL_SCANCODE_Q, SDL_KMOD_LCTRL, window_id);

    return result;
}

static auto get_resource_path() -> std::filesystem::path
{
    Dl_info info {};

    if (dladdr(reinterpret_cast<void*>(&get_resource_path), &info) == 0
        || info.dli_fname == nullptr) {
        throw std::runtime_error("Cannot locate shared library");
    }

    return std::filesystem::path(info.dli_fname).parent_path() / "resources";
}

TEST_CASE("L. Test Line plot")
{
    unsigned int epochs_amount { 10 };
    auto vudiplot { Vudiplot<VudiInit::LinePlot>() };
    std::vector<double> epochs(epochs_amount);
    std::iota(epochs.begin(), epochs.end(), 1);

    VudiInit::VudiplotColors vudiplot_colors;
    ImPlotSpec implot_spec_red;
    implot_spec_red.Marker = ImPlotMarker_Circle;
    implot_spec_red.MarkerSize = 4.0f;
    implot_spec_red.LineColor = vudiplot_colors.red;

    ImPlotSpec spec_blue;
    spec_blue.Marker = ImPlotMarker_Square;
    spec_blue.MarkerSize = 6.0;
    spec_blue.LineColor = vudiplot_colors.blue;

    auto line_plot_init = VudiInit::LinePlot();
    line_plot_init.main_plot_name = "Accuracy plot";
    line_plot_init.x_axis = { epochs, epochs };
    line_plot_init.x_axis_name = "Epochs";
    line_plot_init.y_axis = { generate_random_data(epochs_amount),
                              generate_random_data(epochs_amount) };
    line_plot_init.y_axis_name = "Accuracy";
    line_plot_init.legends = { "Training accuracy ", " Validation accuracy " };
    line_plot_init.implot_colormap = ImPlotColormap_Dark;
    line_plot_init.implot_specs = { implot_spec_red, spec_blue };

    REQUIRE_THROWS(vudiplot.show());
    unsigned int period { 2 };
    auto line_plot_future { std::async(std::launch::async, thread_draw_test,
                                       period) };

    auto is_lineplot_set { vudiplot.set_plot_params(line_plot_init) };
    REQUIRE(is_lineplot_set);

    vudiplot.show();

    try {
        auto result { line_plot_future.get() };
        REQUIRE(result);
    } catch (const std::exception& error) {
        std::cout << "Caught exception: " << error.what() << "\n";
    }
    vudiplot.clean_up_resources();
    SDL_Window* sdl_window { get_app_window() };
    REQUIRE(sdl_window == nullptr);
}

TEST_CASE("M. Test heatmaps plot")
{
    auto vudiplot { Vudiplot<VudiInit::HeatmapsPlot>() };
    std::vector<double> values {
        1.,
        3.,
        4.,
        11.,
    };

    auto heatmaps_plot_init { VudiInit::HeatmapsPlot() };

    heatmaps_plot_init.main_plot_name = "Confusion matrix";
    heatmaps_plot_init.values = values;
    heatmaps_plot_init.rows = 2;
    heatmaps_plot_init.cols = 2;
    heatmaps_plot_init.x_labels.insert(
        heatmaps_plot_init.x_labels.end(),
        { "Predicted positive", "Predicted negative" });
    heatmaps_plot_init.y_labels.insert(
        heatmaps_plot_init.y_labels.end(),
        { "Actual positive", "Actual negative" });
    heatmaps_plot_init.scale_min = 1;
    heatmaps_plot_init.scale_max = 11;
    heatmaps_plot_init.label_fmt = "%g";
    heatmaps_plot_init.implot_colormap = ImPlotColormap_Hot;
    VudiInit::VudiplotColors vudi_colors;

    REQUIRE_THROWS(vudiplot.show());
    unsigned int period { 2 };
    auto heatmaps_plot_future { std::async(std::launch::async,
                                           thread_draw_test, period) };

    auto is_heatmaps_set { vudiplot.set_plot_params(heatmaps_plot_init) };
    REQUIRE(is_heatmaps_set);
    vudiplot.show();

    try {
        auto result { heatmaps_plot_future.get() };
        REQUIRE(result);
    } catch (const std::exception& error) {
        std::cout << "Caught exception: " << error.what() << "\n";
    }
    vudiplot.clean_up_resources();
    SDL_Window* sdl_window { get_app_window() };
    REQUIRE(sdl_window == nullptr);
}

TEST_CASE("N. Test demo plot")
{
    auto demo_init { VudiInit::DemoPlot() };
    auto demo_vudi { Vudiplot<VudiInit::DemoPlot>() };

    REQUIRE_THROWS(demo_vudi.show());
    unsigned int period { 2 };
    auto demo_plot_future { std::async(std::launch::async, thread_draw_test,
                                       period) };

    auto is_demo_set { demo_vudi.set_plot_params(demo_init) };
    REQUIRE(is_demo_set);
    demo_vudi.show();

    try {
        auto result { demo_plot_future.get() };
        REQUIRE(result);
    } catch (const std::exception& error) {
        std::cout << "Caught exception: " << error.what() << "\n";
    }
    demo_vudi.clean_up_resources();
    SDL_Window* sdl_window { get_app_window() };
    REQUIRE(sdl_window == nullptr);
}

static void load_image(const char* filename,
                       std::vector<std::vector<unsigned char>>& images,
                       std::vector<std::size_t>& images_width,
                       std::vector<std::size_t>& images_height)
{
    std::vector<unsigned char> png;
    std::vector<unsigned char> image;
    unsigned width, height;

    unsigned error = lodepng::load_file(png, filename);
    if (!error) {
        error = lodepng::decode(image, width, height, png);
        if (!error) {
            images.emplace_back(image);
            images_width.emplace_back(width);
            images_height.emplace_back(height);
        }
    }

    if (error) {
        std::cerr << "decoder error " << error << ": "
                  << lodepng_error_text(error) << "\n";
    }
}

TEST_CASE("O. Test image plot")
{

    const std::string resources_dir { get_resource_path().string() };
    const std::string png_texture_path { resources_dir + "/textures/images" };

    std::vector<std::filesystem::path> image_paths;
    try {
        if (std::filesystem::exists(png_texture_path)
            && std::filesystem::is_directory(png_texture_path)) {
            for (const auto& file_path :
                 std::filesystem::directory_iterator(png_texture_path)) {
                if (file_path.is_regular_file()
                    && file_path.path().extension() == ".png") {
                    image_paths.emplace_back(file_path.path());
                }
            }
        }
    } catch (std::filesystem::filesystem_error const& error) {
        std::cerr << error.what() << '\n';
    }

    std::vector<std::vector<unsigned char>> raw_images;
    std::vector<std::size_t> images_width;
    std::vector<std::size_t> images_heigth;
    std::vector<std::string> labels;

    for (const auto& path : image_paths) {
        load_image(path.c_str(), raw_images, images_width, images_heigth);
        const std::string& image_label { path.stem().string() };
        labels.emplace_back(image_label);
    }

    VudiInit::VudiplotColors vudi_colors;
    auto blue_trans = vudi_colors.blue;
    blue_trans.w = 0;
    auto vudiplot { Vudiplot<VudiInit::ImagePlot>() };
    auto image_plot_init { VudiInit::ImagePlot() };
    image_plot_init.main_plot_name = "Images";
    image_plot_init.raw_images = raw_images;
    image_plot_init.image_labels = labels;
    image_plot_init.images_width = images_width;
    image_plot_init.images_height = images_heigth;
    image_plot_init.columns = 5;
    image_plot_init.hor_gap = 20;
    image_plot_init.vert_gap = 50;
    image_plot_init.label_bg = blue_trans;

    REQUIRE_THROWS(vudiplot.show());
    unsigned int period { 2 };
    auto demo_plot_future { std::async(std::launch::async, thread_draw_test,
                                       period) };

    auto is_image_set { vudiplot.set_plot_params(image_plot_init) };
    REQUIRE(is_image_set);
    vudiplot.show();

    try {
        auto result { demo_plot_future.get() };
        REQUIRE(result);
    } catch (const std::exception& error) {
        std::cout << "Caught exception: " << error.what() << "\n";
    }
    vudiplot.clean_up_resources();
    SDL_Window* sdl_window { get_app_window() };
    REQUIRE(sdl_window == nullptr);
}

TEST_CASE("P. Test move vudiplot")
{
    auto vudiplot { Vudiplot<VudiInit::HeatmapsPlot>() };
    std::vector<double> values { generate_random_data(36) };

    auto heatmaps_plot_init { VudiInit::HeatmapsPlot() };

    heatmaps_plot_init.main_plot_name = "Confusion matrix";
    heatmaps_plot_init.values = values;
    heatmaps_plot_init.rows = 6;
    heatmaps_plot_init.cols = 6;
    heatmaps_plot_init.label_fmt = "%g";
    heatmaps_plot_init.implot_colormap = ImPlotColormap_Hot;
    VudiInit::VudiplotColors vudi_colors;

    REQUIRE_THROWS(vudiplot.show());
    unsigned int period { 2 };
    auto heatmaps_plot_future { std::async(std::launch::async,
                                           thread_draw_test, period) };

    auto is_heatmaps_set { vudiplot.set_plot_params(heatmaps_plot_init) };
    REQUIRE(is_heatmaps_set);
    auto vudimove { std::move(vudiplot) };

    vudimove.show();
    try {
        auto result { heatmaps_plot_future.get() };
        REQUIRE(result);
    } catch (const std::exception& error) {
        std::cout << "Caught exception: " << error.what() << "\n";
    }
    vudimove.clean_up_resources();
    SDL_Window* sdl_window { get_app_window() };
    REQUIRE(sdl_window == nullptr);
}
