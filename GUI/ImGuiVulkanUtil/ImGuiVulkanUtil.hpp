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

// This is the main class that will creates all apropriate resources
// for Dear ImGui and contains the render engine for drawing the GUI.

#pragma once

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS 1
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <imgui.h>
#include <implot.h>
#include <glm/glm.hpp>
#include <GenPresentation.hpp>
#include <ImGuiTextureManager.hpp>
#include <InitUtils.hpp>

#include <vector>
#include <fstream>
#include <memory>
#include <concepts>

struct ShaderInitInfo {
    const std::string spv_shader_path;
    const std::string vert_func_name;
    const std::string frag_func_name;
};

class ImGuiVulkanUtil {
public:
    ImGuiVulkanUtil(const std::unique_ptr<GenPresentation>& gen_presentation,
                    const std::unique_ptr<ShaderInitInfo>& shader_init_info,
                    const uint32_t max_frames_in_flight,
                    const std::string& texture_path);
    ~ImGuiVulkanUtil();

    [[nodiscard]] auto init_ImGui(float width, float height,
                                  const std::string& font_path) -> bool;
    [[nodiscard]] auto init_resources() -> bool;

    template <std::derived_from<VudiInit::PlotInit> T>
    [[nodiscard]] auto plot_frame(const std::unique_ptr<T>& plot_param,
                                  bool& is_running) -> bool;

    [[nodiscard]] auto
    draw_frame(std::vector<vk::raii::CommandBuffer>& command_buffers,
               uint32_t& frame_index, bool& framebuffer_resized) -> bool;

    void handle_key_press(SDL_Event& event, bool& is_running,
                          bool& framebuffer_resized);

    [[nodiscard]] auto
    load_plot_images(const std::vector<std::vector<unsigned char>>& raw_images,
                     const std::vector<std::size_t>& images_width,
                     const std::vector<std::size_t>& images_height) -> bool;

private:
    // Main Vulkan resources
    const std::unique_ptr<GenPresentation>& gen_presentation;
    const std::string spv_shader_path;
    const std::string vert_func_name;
    const std::string frag_func_name;
    const uint32_t max_frames_in_flight;

    // Vulkan resources for ImGui
    vk::raii::DescriptorPool font_descriptor_pool { nullptr };
    vk::raii::DescriptorSetLayout font_descriptor_set_layout { nullptr };
    vk::raii::DescriptorSet font_descriptor_set { nullptr };
    vk::raii::PipelineCache pipeline_cache { nullptr };
    vk::raii::PipelineLayout pipeline_layout { nullptr };
    vk::raii::Pipeline pipeline { nullptr };
    vk::raii::Sampler font_sampler { nullptr };
    vk::raii::Image font_image { nullptr };
    vk::raii::DeviceMemory font_memory { nullptr };
    vk::raii::ImageView font_image_view { nullptr };

    // Per-frame dynamic buffers to avoid GPU/CPU contention when frames
    // are in flight
    std::vector<vk::raii::Buffer> vertex_buffers;
    std::vector<vk::raii::DeviceMemory> vertex_buffer_memories;
    std::vector<vk::raii::Buffer> index_buffers;
    std::vector<vk::raii::DeviceMemory> index_buffer_memories;
    std::vector<uint32_t> vertex_counts;
    std::vector<uint32_t> index_counts;

    ImGuiContext* imgui_context { nullptr };
    ImPlotContext* implot_context { nullptr };

    struct {
        bool show_imgui_demo = false;
        bool show_imgui_user_guide = false;
        bool show_imgui_style_editor = false;
        bool show_imgui_about_window = false;
        bool show_imgui_metrics_window = false;
    } imgui_demo_state;

    struct {
        bool show_implot_demo = false;
        bool show_implot_user_guide = false;
        bool show_implot_style_editor = false;
        bool show_implot_metrics_window = false;
        bool show_implot_colormap_selector = false;
    } implot_demo_state;

    // UI state management and rendering configuration
    // These members control the visual appearance and dynamic behavior of
    // the UI system
    enum class GuiStyles : uint8_t {
        Vulkan_style = 0,
        Classic,
        Dark,
        Light,
        Glassy,
        Items_count,
    };
    ImGuiStyle vulkan_style;
    ImGuiStyle glassy_style;
    GuiStyles style_num = GuiStyles::Glassy;
    int theme_button { static_cast<int>(style_num) };
    std::size_t selected_language = 0;
    std::string cur_lang_code { "en_US" };

    // Colors
    VudiInit::VudiplotColors vudiplot_colors;

    std::unordered_map<std::string, std::string> language_codes {
        { "English",              "en_US" },
        { "Українська", "uk_UA" },
    };

    struct PushConstBlock {
        glm::vec2 scale;
        glm::vec2 translate;
    } push_const_block;

    std::vector<vk::raii::Semaphore> present_complete_semaphore;
    std::vector<vk::raii::Semaphore> render_finished_semaphore;
    std::vector<vk::raii::Fence> draw_fence;

    // Resources for user texture processing
    struct ImGuiVulkanTexture {
        vk::raii::Image image { nullptr };
        vk::raii::DeviceMemory image_memory { nullptr };
        vk::raii::ImageView image_view { nullptr };
        vk::raii::Sampler sampler { nullptr };
        vk::raii::DescriptorSet descriptor_set { nullptr };
    };

    const std::string& texture_path;
    // This descriptor pool sizes  must be equal or greater of total amount
    // of images loaded from files
    vk::raii::DescriptorPool loaded_textures_descriptor_pool { nullptr };

    // This descriptor pool sizes must be equal or greater of total amount
    // of images loaded from files plus total amount of fonts available in
    // Dear ImGui
    uint32_t total_draw_pool_size { 1 };
    vk::raii::DescriptorPool draw_tex_descriptor_pool { nullptr };
    std::unique_ptr<ImGuiTextureManager> texture_manager;
    vk::raii::DescriptorPool plot_images_descriptor_pool { nullptr };
    std::unique_ptr<ImGuiTextureManager> plot_images_manager;

    std::unordered_map<ImTextureData*, ImGuiVulkanTexture> icon_textures;

    const ImVec2 menubar_icon_size { 36, 36 };

    // Filters for saving file dialog
    static constexpr SDL_DialogFileFilter save_filters[] = {
        { "PNG images", "png" },
        { "All files",  "*"   }
    };

    struct SaveImageConfig {
        bool save_current_image { false };
        std::string screenshot_path { "" };
    };

    SaveImageConfig save_image_config;
    bool screenshot_is_saved { false };
    vk::raii::Buffer screenshot_buffer { nullptr };
    vk::raii::DeviceMemory screenshot_buffer_memory { nullptr };

    void create_vertex_buffers(const vk::DeviceSize buffer_size);
    void create_index_buffers(const vk::DeviceSize buffer_size);

    [[nodiscard]] auto create_image(const vk::Extent2D& extent,
                                    const vk::Format format)
        -> std::pair<vk::raii::Image, vk::raii::DeviceMemory>;

    [[nodiscard]] auto
    create_image_view(const vk::raii::Image& image,
                      const vk::Format format) -> vk::raii::ImageView;

    void create_staging_buffer(const vk::DeviceSize& upload_size,
                               unsigned char* cur_data, const int tex_width,
                               const int tex_height, vk::raii::Image& image);

    [[nodiscard]] auto create_sampler() -> vk::raii::Sampler;

    [[nodiscard]] auto create_descriptor_pool(const uint32_t descr_count,
                                              const uint32_t max_sets)
        -> vk::raii::DescriptorPool;

    [[nodiscard]] auto
    create_descriptor_set_layout() -> vk::raii::DescriptorSetLayout;

    [[nodiscard]] auto create_descriptor_set(
        const vk::raii::DescriptorSetLayout& descr_set_layout,
        const vk::raii::DescriptorPool& descr_pool) -> vk::raii::DescriptorSet;

    void update_descriptor_set(const vk::raii::Sampler& sampler,
                               const vk::raii::ImageView& image_view,
                               vk::raii::DescriptorSet& descr_set);
    void create_pipeline_layout();
    static auto
    read_spv_file(const std::string& filename) -> std::vector<char>;
    [[nodiscard]] auto create_shader_module(
        const std::vector<char>& bytecode) const -> vk::raii::ShaderModule;

    void create_graphics_pipeline();
    void set_style(GuiStyles gui_style);

    void update_buffers(const uint32_t frame_index);
    void transition_image_layout(vk::Image image, vk::ImageLayout old_layout,
                                 vk::ImageLayout new_layout,
                                 vk::AccessFlags2 src_access_mask,
                                 vk::AccessFlags2 dst_access_mask,
                                 vk::PipelineStageFlags2 src_stage_mask,
                                 vk::PipelineStageFlags2 dst_stage_mask,
                                 vk::ImageAspectFlags image_aspect_flags,
                                 vk::raii::CommandBuffer& command_buffer);

    void record_command_buffer(
        std::vector<vk::raii::CommandBuffer>& command_buffers,
        const uint32_t image_index, const uint32_t frame_index);
    void create_semaphores_and_fences();

    void update_texture(ImTextureData* tex);
    auto menu_item_with_icon(ImTextureID icon, const ImVec2& icon_size,
                             const char* label, const char* shortcut = nullptr,
                             bool enabled = true) -> bool;

    // Functions for style changing
    void set_up_vulkan_style();
    void set_up_glassy_style();
    void add_background_to_menu(const ImTextureID& image);
    void draw_demos();
    [[nodiscard]] auto load_textures() -> bool;
    void center_widget_width_position(const float widget_width);
    void center_widget_height_position(const float widget_height);
    [[nodiscard]] auto create_menubar(bool& is_running) -> bool;
    [[nodiscard]] auto
    line_plot(const std::unique_ptr<VudiInit::LinePlot>& plot_param) -> bool;

    [[nodiscard]] auto heatmaps_plot(
        const std::unique_ptr<VudiInit::HeatmapsPlot>& plot_param) -> bool;
    [[nodiscard]] auto
    image_plot(const std::unique_ptr<VudiInit::ImagePlot>& plot_param) -> bool;

    void clean_up();

    [[nodiscard]] auto show_demos(bool& is_running) -> bool;
    static void SDLCALL save_file_callback(void* userdata,
                                           const char* const* filelist,
                                           int filter);
    void save_screenshot_dialog();
    [[nodiscard]] auto create_screenshot_buffer_and_memory()
        -> std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>;
    [[nodiscard]] auto
    save_screenshot_buffer_to_file(uint32_t frame_index) -> bool;
    void save_screenshot_success_window(const std::string& modal_window_name);
};
