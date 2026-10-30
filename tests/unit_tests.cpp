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

#include <GenInstance.hpp>
#include <GenPresentation.hpp>
#include <GenCommandBuffer.hpp>
#include <GenSDLWindow.hpp>
#include <BufferUtils.hpp>
#include <InitUtils.hpp>
#include <GenGettext.hpp>
#include <ImGuiTextureMesh.hpp>
#include <lodepng.h>
#include <ImGuiTextureManager.hpp>
#include <ImGuiVulkanUtil.hpp>
#include <Vudiplot.hpp>

#include <memory>
#include <string>
#include <iostream>
#include <type_traits>
#include <concepts>
#include <vector>
#include <filesystem>
#include <random>
#include <numeric>
#include <dlfcn.h>

struct RGBAImage {
    std::vector<unsigned char> image;
    uint32_t width;
    uint32_t height;
};

static auto load_image(const char* filename) -> RGBAImage

{
    std::vector<unsigned char> png;
    std::vector<unsigned char> image; // the raw pixels
    unsigned width, height;
    RGBAImage rgba_image;
    // load and decode
    unsigned error = lodepng::load_file(png, filename);
    if (!error) {
        error = lodepng::decode(image, width, height, png);
        if (!error) {
            rgba_image.image = image;
            rgba_image.width = width;
            rgba_image.height = height;
        }
    }

    if (error) {
        std::cerr << "decoder error " << error << ": "
                  << lodepng_error_text(error) << "\n";
    }

    return rgba_image;
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

TEST_CASE("A. Test SDL window")
{
    const std::string& window_name { "Test_Vulkan" };
    constexpr uint32_t width { 800 };
    constexpr uint32_t height { 600 };
    auto gen_sdl_window { std::make_unique<GenSDLWindow>() };
    gen_sdl_window->init_sdl_window(window_name, width, height);
    auto sdl_window { gen_sdl_window->get_sdl_window() };
    REQUIRE(sdl_window != nullptr);
}

TEST_CASE("B. Test gettext")
{
    const std::string resources_dir { get_resource_path().string() };

    auto gen_gettext { std::make_unique<GenGettext>(resources_dir) };
    auto is_init { gen_gettext->init() };
    REQUIRE(is_init);

    setenv("LANGUAGE", "en_US", 1);
    setlocale(LC_ALL, "en_US.utf8");
    REQUIRE(std::string { gettext("Exit") } == "Exit");

    setenv("LANGUAGE", "uk_UA", 1);
    setlocale(LC_ALL, "uk_UA.utf8");
    REQUIRE(std::string { gettext("Exit") } == "Вихід");
}

TEST_CASE("C. Test GenInstance")
{
    const std::string& window_name { "Test_Vulkan" };
    constexpr uint32_t width { 800 };
    constexpr uint32_t height { 600 };
    auto gen_sdl_window { std::make_unique<GenSDLWindow>() };
    gen_sdl_window->init_sdl_window(window_name, width, height);

    const std::string app_name = "Vudiplot API";
    auto gen_instance { std::make_unique<GenInstance>(app_name) };
    REQUIRE(gen_instance != nullptr);

    const auto& cur_physical_device { gen_instance->get_physical_device() };
    REQUIRE(cur_physical_device != nullptr);

    const auto& instance { gen_instance->get_instance() };
    REQUIRE(instance != nullptr);
}

TEST_CASE("D. Test GenPresentation")
{
    const std::string& window_name { "Test_Vulkan" };
    constexpr uint32_t width { 800 };
    constexpr uint32_t height { 600 };
    auto gen_sdl_window { std::make_unique<GenSDLWindow>() };
    gen_sdl_window->init_sdl_window(window_name, width, height);
    auto sdl_window { gen_sdl_window->get_sdl_window() };

    const std::string app_name = "Vudiplot API";
    auto gen_instance { std::make_unique<GenInstance>(app_name) };
    const auto& cur_physical_device { gen_instance->get_physical_device() };
    const auto& instance { gen_instance->get_instance() };

    auto gen_presentation { std::make_unique<GenPresentation>(
        instance, sdl_window, cur_physical_device) };
    REQUIRE(gen_presentation != nullptr);

    const auto& logical_device { gen_presentation->get_logical_device() };
    REQUIRE(logical_device != nullptr);

    const auto& presentation_physical_device {
        gen_presentation->get_physical_device()
    };
    REQUIRE(presentation_physical_device != nullptr);

    auto presentation_sdl_window { gen_presentation->get_sdl_window() };
    REQUIRE(presentation_sdl_window != nullptr);

    const auto& presentation_instance { gen_presentation->get_instance() };
    REQUIRE(presentation_instance != nullptr);

    auto extent { gen_presentation->get_swapchain_extent() };
    REQUIRE(extent.width == width);
    REQUIRE(extent.height == height);

    auto& presentation_queue { gen_presentation->get_presentation_queue() };
    REQUIRE(presentation_queue != nullptr);

    const auto& swapchain { gen_presentation->get_swapchain() };
    REQUIRE(swapchain != nullptr);

    const auto queue_index { gen_presentation->get_queue_index() };
    REQUIRE(queue_index >= 0);

    const auto& swapchain_images { gen_presentation->get_swapchain_images() };
    REQUIRE(!swapchain_images.empty());

    const auto& swapchain_image_views {
        gen_presentation->get_swapchain_image_views()
    };
    REQUIRE(!swapchain_image_views.empty());

    const auto& color_image { gen_presentation->get_color_image() };
    REQUIRE(color_image != nullptr);

    const auto& color_image_views { gen_presentation->get_color_image_view() };
    REQUIRE(color_image_views != nullptr);

    const auto& depth_image { gen_presentation->get_depth_image() };
    REQUIRE(depth_image != nullptr);

    const auto& depth_image_views { gen_presentation->get_depth_image_view() };
    REQUIRE(depth_image_views != nullptr);

    auto depth_format { gen_presentation->get_depth_format() };
    REQUIRE(std::is_same_v<decltype(depth_format), vk::Format>);

    auto msaa_samples { gen_presentation->get_msaa_samples() };
    REQUIRE(std::is_same_v<decltype(msaa_samples), vk::SampleCountFlagBits>);
}

TEST_CASE("E. Test GenCommandBuffer")
{
    const std::string& window_name { "Test_Vulkan" };
    constexpr uint32_t width { 800 };
    constexpr uint32_t height { 600 };
    auto gen_sdl_window { std::make_unique<GenSDLWindow>() };
    gen_sdl_window->init_sdl_window(window_name, width, height);
    auto sdl_window { gen_sdl_window->get_sdl_window() };

    const std::string app_name = "Vudiplot API";
    auto gen_instance { std::make_unique<GenInstance>(app_name) };
    const auto& cur_physical_device { gen_instance->get_physical_device() };
    const auto& instance { gen_instance->get_instance() };

    auto gen_presentation { std::make_unique<GenPresentation>(
        instance, sdl_window, cur_physical_device) };

    const auto& logical_device { gen_presentation->get_logical_device() };

    const auto queue_index { gen_presentation->get_queue_index() };

    const uint32_t max_frames_in_flight { 2 };
    const auto& gen_command_buffer { std::make_unique<GenCommandBuffer>(
        queue_index, logical_device, max_frames_in_flight) };
    REQUIRE(gen_command_buffer != nullptr);

    const auto& command_buffers { gen_command_buffer->get_command_buffers() };
    REQUIRE(!command_buffers.empty());

    const auto frames { gen_command_buffer->get_max_frame_in_flight() };
    REQUIRE(frames == 2);
}

TEST_CASE("F. Test BufferUtils")
{
    const std::string& window_name { "Test_Vulkan" };
    constexpr uint32_t width { 800 };
    constexpr uint32_t height { 600 };
    auto gen_sdl_window { std::make_unique<GenSDLWindow>() };
    gen_sdl_window->init_sdl_window(window_name, width, height);
    auto sdl_window { gen_sdl_window->get_sdl_window() };

    const std::string app_name = "Vudiplot API";
    auto gen_instance { std::make_unique<GenInstance>(app_name) };
    const auto& cur_physical_device { gen_instance->get_physical_device() };
    const auto& instance { gen_instance->get_instance() };

    auto gen_presentation { std::make_unique<GenPresentation>(
        instance, sdl_window, cur_physical_device) };
    const auto& logical_device { gen_presentation->get_logical_device() };
    auto& presentation_queue { gen_presentation->get_presentation_queue() };
    const auto queue_index { gen_presentation->get_queue_index() };

    const vk::PhysicalDeviceMemoryProperties& physical_mem_properties {
        cur_physical_device.getMemoryProperties()
    };
    auto buffer_utils { std::make_unique<BufferUtils>(
        physical_mem_properties, logical_device, queue_index) };
    REQUIRE(buffer_utils != nullptr);

    const vk::DeviceSize buffer_size { 1024 };
    auto [buffer, buffer_memory] { buffer_utils->create_buffer(
        buffer_size, vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible
            | vk::MemoryPropertyFlagBits::eHostCoherent) };
    REQUIRE(buffer != nullptr);
    REQUIRE(buffer_memory != nullptr);

    auto [image, image_memory] { buffer_utils->create_image(
        width, height, vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eSampled
            | vk::ImageUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal, 1,
        vk::SampleCountFlagBits::e1) };
    REQUIRE(image != nullptr);
    REQUIRE(image_memory != nullptr);

    const auto& image_view { buffer_utils->create_imageview(
        image, vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlagBits::eColor,
        1) };
    REQUIRE(image_view != nullptr);

    const vk::DeviceSize src_buffer_size { width * height * 4 };
    auto [src_buffer, src_buffer_memory] { buffer_utils->create_buffer(
        src_buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible
            | vk::MemoryPropertyFlagBits::eHostCoherent) };

    auto [dst_image, dst_image_memory] { buffer_utils->create_image(
        width, height, vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eSampled
            | vk::ImageUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal, 1,
        vk::SampleCountFlagBits::e1) };

    auto is_success { buffer_utils->transition_image_layout(
        dst_image, vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal, 1,
        gen_presentation->get_presentation_queue()) };
    REQUIRE(is_success == true);

    buffer_utils->copy_buffer_to_image(src_buffer, dst_image, width, height,
                                       presentation_queue);
    REQUIRE(dst_image != nullptr);
}

TEST_CASE("G. Test InitUtils")
{
    std::cout << "Testing InitUtils\n";
    auto line_plot { VudiInit::LinePlot() };
    REQUIRE(std::is_same_v<decltype(line_plot), VudiInit::LinePlot>);
    REQUIRE(std::derived_from<decltype(line_plot), VudiInit::PlotInit>);

    auto heatmaps_plot { VudiInit::HeatmapsPlot() };
    REQUIRE(std::is_same_v<decltype(heatmaps_plot), VudiInit::HeatmapsPlot>);
    REQUIRE(std::derived_from<decltype(heatmaps_plot), VudiInit::PlotInit>);

    auto image_plot { VudiInit::ImagePlot() };
    REQUIRE(std::is_same_v<decltype(image_plot), VudiInit::ImagePlot>);
    REQUIRE(std::derived_from<decltype(image_plot), VudiInit::PlotInit>);

    auto plot_init { VudiInit::PlotInit() };
    REQUIRE(std::is_same_v<decltype(plot_init), VudiInit::PlotInit>);

    auto demo_plot { VudiInit::DemoPlot() };
    REQUIRE(std::is_same_v<decltype(demo_plot), VudiInit::DemoPlot>);
    REQUIRE(std::derived_from<decltype(demo_plot), VudiInit::PlotInit>);

    auto vudi_colors { VudiInit::VudiplotColors() };
    REQUIRE(std::is_same_v<decltype(vudi_colors), VudiInit::VudiplotColors>);
}

TEST_CASE("I. Test ImGuiTextureMesh")
{
    const std::string& window_name { "Test_Vulkan" };
    constexpr uint32_t width { 800 };
    constexpr uint32_t height { 600 };
    auto gen_sdl_window { std::make_unique<GenSDLWindow>() };
    gen_sdl_window->init_sdl_window(window_name, width, height);
    auto sdl_window { gen_sdl_window->get_sdl_window() };

    const std::string app_name = "Vudiplot API";
    auto gen_instance { std::make_unique<GenInstance>(app_name) };
    const auto& cur_physical_device { gen_instance->get_physical_device() };
    const auto& instance { gen_instance->get_instance() };

    auto gen_presentation { std::make_unique<GenPresentation>(
        instance, sdl_window, cur_physical_device) };
    const auto& logical_device { gen_presentation->get_logical_device() };
    auto& presentation_queue { gen_presentation->get_presentation_queue() };
    const auto queue_index { gen_presentation->get_queue_index() };

    const std::string resources_dir { get_resource_path().string() };
    const std::string& tex_path { resources_dir
                                  + "/textures/KTX_images/about.ktx2" };
    auto mesh_init_file { std::make_unique<MeshInitInfoFile>(
        MeshInitInfoFile { .texture_path = tex_path,
                           .physical_device = cur_physical_device,
                           .logical_device = logical_device,
                           .queue_index = queue_index,
                           .graphics_queue = presentation_queue }) };

    auto texture_mesh { std::make_unique<ImGuiTextureMesh>(mesh_init_file) };

    auto mesh_image { texture_mesh->get_texture_image() };
    REQUIRE(mesh_image != nullptr);

    auto mesh_image_memory { texture_mesh->get_texture_image_memory() };
    REQUIRE(mesh_image_memory != nullptr);

    auto mesh_image_view { texture_mesh->get_texture_image_view() };
    REQUIRE(mesh_image_view != nullptr);

    auto mesh_sampler { texture_mesh->get_texture_sampler() };
    REQUIRE(mesh_sampler != nullptr);

    auto mesh_image_size { texture_mesh->get_image_size() };
    REQUIRE(mesh_image_size.image_width > 0);
    REQUIRE(mesh_image_size.image_height > 0);

    const std::string& rgba_image_path { resources_dir
                                         + "/textures/images/about.png" };
    RGBAImage rgba_image { load_image(rgba_image_path.c_str()) };

    auto mesh_init_rgba { std::make_unique<MeshInitInfoRawChar>(
        MeshInitInfoRawChar { .rgba_image = rgba_image.image,
                              .image_width = rgba_image.width,
                              .image_height = rgba_image.height,
                              .physical_device = cur_physical_device,
                              .logical_device = logical_device,
                              .queue_index = queue_index,
                              .graphics_queue = presentation_queue }) };
    auto rgba_texture_mesh { std::make_unique<ImGuiTextureMesh>(
        mesh_init_rgba) };

    auto rgba_mesh_image { rgba_texture_mesh->get_texture_image() };
    REQUIRE(rgba_mesh_image != nullptr);

    auto rgba_mesh_image_memory {
        rgba_texture_mesh->get_texture_image_memory()
    };
    REQUIRE(rgba_mesh_image_memory != nullptr);

    auto rgba_mesh_image_view { rgba_texture_mesh->get_texture_image_view() };
    REQUIRE(rgba_mesh_image_view != nullptr);

    auto rgba_mesh_sampler { rgba_texture_mesh->get_texture_sampler() };
    REQUIRE(rgba_mesh_sampler != nullptr);

    auto rgba_mesh_image_size { rgba_texture_mesh->get_image_size() };
    REQUIRE(rgba_mesh_image_size.image_width == rgba_image.width);
    REQUIRE(rgba_mesh_image_size.image_height == rgba_image.height);
}

auto get_image_paths(const std::string& texture_path,
                     const std::string& extension)
    -> std::vector<std::filesystem::path>
{
    std::vector<std::filesystem::path> image_paths;
    try {
        if (std::filesystem::exists(texture_path)
            && std::filesystem::is_directory(texture_path)) {
            for (const auto& file_path :
                 std::filesystem::directory_iterator(texture_path)) {
                if (file_path.is_regular_file()
                    && file_path.path().extension() == extension) {
                    image_paths.emplace_back(file_path.path());
                }
            }
        }
    } catch (std::filesystem::filesystem_error const& error) {
        std::cerr << error.what() << '\n';
    }

    return image_paths;
}

auto create_descriptor_pool(const uint32_t images_amount,
                            const vk::raii::Device& logical_device)
    -> vk::raii::DescriptorPool
{

    vk::DescriptorPoolSize descr_pool_size {
        .type = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = images_amount
    };

    vk::DescriptorPoolCreateInfo descr_pool_info {
        .flags = vk::DescriptorPoolCreateFlagBits::
            eFreeDescriptorSet, // Allow individual descriptor set freeing
        .maxSets = images_amount, // Maximum number of descriptor sets
        .poolSizeCount = 1, // Number of pool size specifications
        .pPoolSizes = &descr_pool_size // size configuration
    };
    return vk::raii::DescriptorPool(logical_device, descr_pool_info);
}

TEST_CASE("J. Test ImGuiTextureManager")
{
    const std::string& window_name { "Test_Vulkan" };
    constexpr uint32_t width { 800 };
    constexpr uint32_t height { 600 };
    auto gen_sdl_window { std::make_unique<GenSDLWindow>() };
    gen_sdl_window->init_sdl_window(window_name, width, height);
    auto sdl_window { gen_sdl_window->get_sdl_window() };

    const std::string app_name = "Vudiplot API";
    auto gen_instance { std::make_unique<GenInstance>(app_name) };
    const auto& cur_physical_device { gen_instance->get_physical_device() };
    const auto& instance { gen_instance->get_instance() };

    auto gen_presentation { std::make_unique<GenPresentation>(
        instance, sdl_window, cur_physical_device) };
    const auto& logical_device { gen_presentation->get_logical_device() };
    auto& presentation_queue { gen_presentation->get_presentation_queue() };
    const auto queue_index { gen_presentation->get_queue_index() };

    const std::string resources_dir { get_resource_path().string() };
    const std::string& texture_path { resources_dir
                                      + "/textures/KTX_images/" };
    auto ktx_image_paths { get_image_paths(texture_path, ".ktx2") };
    auto ktx_images_amount { static_cast<uint32_t>(ktx_image_paths.size()) };
    REQUIRE(ktx_images_amount != 0);

    auto descr_pool { create_descriptor_pool(ktx_images_amount,
                                             logical_device) };

    auto ktx_texture_manager { std::make_unique<ImGuiTextureManager>(
        logical_device, descr_pool) };
    REQUIRE(ktx_texture_manager != nullptr);

    for (const auto& path : ktx_image_paths) {

        auto mesh_info { std::make_unique<MeshInitInfoFile>(MeshInitInfoFile {
            .texture_path = path.string(),
            .physical_device = gen_presentation->get_physical_device(),
            .logical_device = gen_presentation->get_logical_device(),
            .queue_index = gen_presentation->get_queue_index(),
            .graphics_queue = gen_presentation->get_presentation_queue() })

        };

        auto texture_mesh { std::make_unique<ImGuiTextureMesh>(mesh_info) };

        const std::string& tex_name { path.stem().string() };
        ktx_texture_manager->register_texture(
            texture_mesh->get_texture_image(),
            texture_mesh->get_texture_image_memory(),
            texture_mesh->get_texture_image_view(),
            texture_mesh->get_texture_sampler(),
            texture_mesh->get_image_size().image_width,
            texture_mesh->get_image_size().image_height, tex_name);
    }

    for (const auto& path : ktx_image_paths) {
        const std::string tex_name { path.stem().string() };
        auto ktx_texture { ktx_texture_manager->get_texture(tex_name) };

        REQUIRE(std::is_same_v<decltype(ktx_texture), ImTextureID>);
    }

    const std::string& rgba_texture_path { resources_dir
                                           + "/textures/images/" };
    auto rgba_image_paths { get_image_paths(rgba_texture_path, ".png") };
    auto rgba_images_amount { static_cast<uint32_t>(rgba_image_paths.size()) };
    REQUIRE(rgba_images_amount != 0);
    auto rgba_descr_pool { create_descriptor_pool(rgba_images_amount,
                                                  logical_device) };

    auto rgba_texture_manager { std::make_unique<ImGuiTextureManager>(
        logical_device, rgba_descr_pool) };
    REQUIRE(rgba_texture_manager != nullptr);

    for (const auto& path : rgba_image_paths) {
        auto raw_image { load_image(path.string().c_str()) };

        auto mesh_info {
            std::make_unique<MeshInitInfoRawChar>(MeshInitInfoRawChar {
                .rgba_image = raw_image.image,
                .image_width = raw_image.width,
                .image_height = raw_image.height,
                .physical_device = gen_presentation->get_physical_device(),
                .logical_device = gen_presentation->get_logical_device(),
                .queue_index = gen_presentation->get_queue_index(),
                .graphics_queue = gen_presentation->get_presentation_queue() })

        };

        auto texture_mesh { std::make_unique<ImGuiTextureMesh>(mesh_info) };

        rgba_texture_manager->register_texture(
            texture_mesh->get_texture_image(),
            texture_mesh->get_texture_image_memory(),
            texture_mesh->get_texture_image_view(),
            texture_mesh->get_texture_sampler(),
            texture_mesh->get_image_size().image_width,
            texture_mesh->get_image_size().image_height);
    }

    const auto& rgba_textures { rgba_texture_manager->get_plot_textures() };
    REQUIRE(rgba_textures.size() == rgba_image_paths.size());
}

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

static void load_image(const char* filename,
                       std::vector<std::vector<unsigned char>>& images,
                       std::vector<std::size_t>& images_width,
                       std::vector<std::size_t>& images_height)
{
    std::vector<unsigned char> png;
    std::vector<unsigned char> image; // the raw pixels
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

TEST_CASE("K. Test ImGuiVulkanUtil")
{
    const std::string& window_name { "Test_Vulkan" };
    constexpr uint32_t width { 800 };
    constexpr uint32_t height { 600 };
    auto gen_sdl_window { std::make_unique<GenSDLWindow>() };
    gen_sdl_window->init_sdl_window(window_name, width, height);
    auto sdl_window { gen_sdl_window->get_sdl_window() };

    const std::string app_name = "Vudiplot API";
    auto gen_instance { std::make_unique<GenInstance>(app_name) };
    const auto& cur_physical_device { gen_instance->get_physical_device() };
    const auto& instance { gen_instance->get_instance() };

    auto gen_presentation { std::make_unique<GenPresentation>(
        instance, sdl_window, cur_physical_device) };
    const auto& logical_device { gen_presentation->get_logical_device() };
    auto& presentation_queue { gen_presentation->get_presentation_queue() };
    const auto queue_index { gen_presentation->get_queue_index() };
    const std::string resources_dir { get_resource_path().string() };
    auto imgui_shader_info { std::make_unique<ShaderInitInfo>(ShaderInitInfo {
        .spv_shader_path { resources_dir + "/shaders/imgui.spv" },
        .vert_func_name { "vert_main" },
        .frag_func_name { "frag_main" },

    }) };
    const uint32_t max_frames_in_flight { 2 };
    const std::string& texture_path { resources_dir + "/textures/KTX_images" };
    // Create a class that represents Dear ImGui
    auto imgui_util = std::make_unique<ImGuiVulkanUtil>(
        gen_presentation, imgui_shader_info, max_frames_in_flight,
        texture_path);
    REQUIRE(imgui_util != nullptr);

    const std::string& font_path {
        resources_dir + "/fonts/GeistMonoNerdFontMono-Regular.otf"
    };

    auto is_imgui_init { imgui_util->init_ImGui(width, height, font_path) };
    REQUIRE(is_imgui_init == true);

    auto is_resources_init { imgui_util->init_resources() };
    REQUIRE(is_resources_init == true);

    bool is_running { false };

    SECTION("Test Demo plot")
    {
        auto demo_plot_init { std::make_unique<VudiInit::DemoPlot>() };

        auto is_plot_demo { imgui_util->plot_frame(demo_plot_init,
                                                   is_running) };
        REQUIRE(is_plot_demo == true);
    }

    SECTION("Test Line plot")
    {
        std::vector<double> epochs(10);
        std::iota(epochs.begin(), epochs.end(), 1);

        VudiInit::VudiplotColors vudiplot_colors;
        ImPlotSpec implot_spec_red;
        implot_spec_red.Marker = ImPlotMarker_Circle;
        implot_spec_red.MarkerSize = 4.0f;
        implot_spec_red.LineColor = vudiplot_colors.purple;

        ImPlotSpec spec_blue;
        spec_blue.Marker = ImPlotMarker_Square;
        spec_blue.MarkerSize = 6.0;
        spec_blue.LineColor = vudiplot_colors.blue;

        auto line_plot_init = VudiInit::LinePlot();
        line_plot_init.main_plot_name = "Accuracy plot";
        line_plot_init.x_axis = { epochs, epochs };
        line_plot_init.x_axis_name = "Epochs";
        line_plot_init.y_axis
            = { generate_random_data(10), generate_random_data(10) };
        line_plot_init.y_axis_name = "Accuracy";
        line_plot_init.legends
            = { "Training accuracy ", " Validation accuracy " };
        line_plot_init.implot_colormap = ImPlotColormap_Dark;
        line_plot_init.implot_specs = { spec_blue, implot_spec_red };

        auto line_plot_init_ptr { std::make_unique<VudiInit::LinePlot>(
            line_plot_init) };
        const auto is_line_plot { imgui_util->plot_frame(line_plot_init_ptr,
                                                         is_running) };
        REQUIRE(is_line_plot);

        auto line_plot_init_2 = VudiInit::LinePlot();
        line_plot_init_2.x_axis = { epochs };
        line_plot_init_2.y_axis
            = { generate_random_data(10), generate_random_data(10) };
        auto line_plot_init_ptr_2 { std::make_unique<VudiInit::LinePlot>(
            line_plot_init_2) };

        const auto not_line_plot_dif_axes { imgui_util->plot_frame(
            line_plot_init_ptr_2, is_running) };
        REQUIRE_FALSE(not_line_plot_dif_axes);

        auto line_plot_init_3 = VudiInit::LinePlot();
        line_plot_init_3.x_axis = { epochs, epochs };
        line_plot_init_3.y_axis
            = { generate_random_data(10), generate_random_data(9) };
        line_plot_init_3.implot_colormap = ImPlotColormap_Jet;
        auto line_plot_init_ptr_3 { std::make_unique<VudiInit::LinePlot>(
            line_plot_init_3) };

        const auto not_line_plot_dif_axis { imgui_util->plot_frame(
            line_plot_init_ptr_3, is_running) };
        REQUIRE_FALSE(not_line_plot_dif_axis);
    }

    SECTION("Test Heatmaps plot")
    {
        std::vector<double> values { generate_random_data(4) };

        auto heatmaps_plot_init { VudiInit::HeatmapsPlot() };

        ImPlotSpec heat_spec;

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
        heatmaps_plot_init.label_fmt = "%g";
        heatmaps_plot_init.implot_colormap = ImPlotColormap_Hot;
        VudiInit::VudiplotColors vudi_colors;

        heat_spec.FillColor = vudi_colors.green;
        heatmaps_plot_init.implot_specs = { heat_spec };

        auto heat_init_ptr { std::make_unique<VudiInit::HeatmapsPlot>(
            heatmaps_plot_init) };

        auto is_heatmaps { imgui_util->plot_frame(heat_init_ptr, is_running) };
        REQUIRE(is_heatmaps);

        heatmaps_plot_init.rows = 1;

        auto heat_init_ptr_wrong_rows {
            std::make_unique<VudiInit::HeatmapsPlot>(heatmaps_plot_init)
        };
        auto wrong_rows_heatmaps { imgui_util->plot_frame(
            heat_init_ptr_wrong_rows, is_running) };
        REQUIRE_FALSE(wrong_rows_heatmaps);

        heatmaps_plot_init.rows = 2;
        heatmaps_plot_init.x_labels.pop_back();
        auto wrong_x_labels_heatmaps_init {
            std::make_unique<VudiInit::HeatmapsPlot>(heatmaps_plot_init)
        };
        auto wrong_x_labels { imgui_util->plot_frame(
            wrong_x_labels_heatmaps_init, is_running) };
        REQUIRE_FALSE(wrong_x_labels);

        heatmaps_plot_init.x_labels.emplace_back("Predicted negative");
        heatmaps_plot_init.y_labels.pop_back();
        auto wrong_y_labels_heatmaps_init {
            std::make_unique<VudiInit::HeatmapsPlot>(heatmaps_plot_init)
        };
        auto wrong_y_labels { imgui_util->plot_frame(
            wrong_y_labels_heatmaps_init, is_running) };
        REQUIRE_FALSE(wrong_y_labels);
    }

    SECTION("Test Image plot")
    {
        const std::string resources_dir { get_resource_path().string() };
        const std::string png_texture_path { resources_dir
                                             + "/textures/images" };
        std::vector<std::string> image_paths;
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
            size_t last_index { path.find_last_of(".") };
            size_t first_index { path.find_last_of("/") + 1 };
            const std::string& image_label { path.substr(
                first_index, last_index - first_index) };

            labels.emplace_back(image_label);
        }

        VudiInit::VudiplotColors vudi_colors;
        auto blue_trans = vudi_colors.blue;
        blue_trans.w = 0;

        auto image_plot_init { VudiInit::ImagePlot() };

        image_plot_init.raw_images = raw_images;
        image_plot_init.image_labels = labels;
        image_plot_init.images_width = images_width;
        image_plot_init.images_height = images_heigth;
        image_plot_init.columns = 5;
        image_plot_init.hor_gap = 20;
        image_plot_init.vert_gap = 50;
        image_plot_init.label_bg = blue_trans;

        auto is_load_plot_images { imgui_util->load_plot_images(
            raw_images, images_width, images_heigth) };
        REQUIRE(is_load_plot_images);

        std::vector<std::vector<unsigned char>> empty_raw_images;
        auto zero_plot_images_to_load { imgui_util->load_plot_images(
            empty_raw_images, images_width, images_heigth) };
        REQUIRE_FALSE(zero_plot_images_to_load);

        images_width.pop_back();
        auto wrong_amount_of_widths { imgui_util->load_plot_images(
            raw_images, images_width, images_heigth) };
        REQUIRE_FALSE(wrong_amount_of_widths);

        raw_images.pop_back();
        images_heigth.pop_back();
        images_heigth.pop_back();
        auto wrong_amount_of_heights { imgui_util->load_plot_images(
            raw_images, images_width, images_heigth) };
        REQUIRE_FALSE(wrong_amount_of_heights);

        auto image_plot_init_ptr { std::make_unique<VudiInit::ImagePlot>(
            image_plot_init) };
        auto is_image_plot { imgui_util->plot_frame(image_plot_init_ptr,
                                                    is_running) };
        REQUIRE(is_image_plot);
    }

    SECTION("Test draw frame")
    {
        const auto& gen_command_buffer { std::make_unique<GenCommandBuffer>(
            queue_index, logical_device, max_frames_in_flight) };
        uint32_t frame_index { 0 };
        auto is_draw { imgui_util->draw_frame(
            gen_command_buffer->get_command_buffers(), frame_index,
            is_running) };
        REQUIRE(is_draw);
    }
}

TEST_CASE("L. Test Vudiplot")
{
    REQUIRE_NOTHROW(Vudiplot<VudiInit::LinePlot>());
    auto vudiplot_line { Vudiplot<VudiInit::LinePlot>() };
    auto lineplot_init { VudiInit::LinePlot() };
    auto is_set_line { vudiplot_line.set_plot_params(lineplot_init) };
    REQUIRE(is_set_line);
    vudiplot_line.clean_up_resources();

    REQUIRE_NOTHROW(Vudiplot<VudiInit::HeatmapsPlot>());
    auto vudiplot_heatmaps { Vudiplot<VudiInit::HeatmapsPlot>() };
    auto heatmaps_init { VudiInit::HeatmapsPlot() };
    auto is_set_heatmaps { vudiplot_heatmaps.set_plot_params(heatmaps_init) };
    REQUIRE(is_set_heatmaps);
    vudiplot_heatmaps.clean_up_resources();

    REQUIRE_NOTHROW(Vudiplot<VudiInit::ImagePlot>());
    const std::string resources_dir { get_resource_path().string() };
    const std::string png_texture_path { resources_dir + "/textures/images" };
    std::vector<std::string> image_paths;
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
        size_t last_index { path.find_last_of(".") };
        size_t first_index { path.find_last_of("/") + 1 };
        const std::string& image_label { path.substr(
            first_index, last_index - first_index) };

        labels.emplace_back(image_label);
    }

    auto vudiplot_image { Vudiplot<VudiInit::ImagePlot>() };
    auto image_init { VudiInit::ImagePlot() };
    image_init.raw_images = raw_images;
    image_init.images_width = images_width;
    image_init.images_height = images_heigth;
    image_init.image_labels = labels;
    auto is_set_image { vudiplot_image.set_plot_params(image_init) };
    REQUIRE(is_set_image);
    vudiplot_image.clean_up_resources();

    REQUIRE_NOTHROW(Vudiplot<VudiInit::DemoPlot>());
    auto vudiplot_demo { Vudiplot<VudiInit::DemoPlot>() };
    auto demo_init { VudiInit::DemoPlot() };
    auto is_set_demo { vudiplot_demo.set_plot_params(demo_init) };
    REQUIRE(is_set_demo);
    vudiplot_demo.clean_up_resources();
}
