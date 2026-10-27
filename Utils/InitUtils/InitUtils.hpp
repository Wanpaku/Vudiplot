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

#pragma once

#include <vector>
#include <string>
#include <implot.h>

namespace VudiInit {

struct PlotInit {
    std::vector<ImPlotSpec> implot_specs;
    ImPlotColormap implot_colormap { ImPlotColormap_Deep };
    std::string main_plot_name { "##NoTitle" };
};

struct DemoPlot : public PlotInit { };

struct LinePlot : public PlotInit {
    std::vector<std::vector<double>> x_axis;
    std::string x_axis_name { "##NoTitle" };
    std::vector<std::vector<double>> y_axis;
    std::string y_axis_name { "##NoTitle" };
    std::vector<std::string> legends;
};

struct HeatmapsPlot : public PlotInit {
    std::vector<double> values;
    int rows { 1 };
    int cols { 1 };
    double scale_min { 0 };
    double scale_max { 0 };
    std::string label_fmt { "%g" };
    ImPlotPoint bounds_min = ImPlotPoint(0, 0);
    ImPlotPoint bounds_max = ImPlotPoint(1, 1);
    std::vector<const char*> x_labels;
    std::vector<const char*> y_labels;
};

struct ImagePlot : public PlotInit {
    unsigned int columns { 0 };
    double hor_gap { 0 };
    double vert_gap { 0 };
    std::vector<std::string> image_labels;
    std::vector<std::vector<unsigned char>> raw_images;
    std::vector<std::size_t> images_width;
    std::vector<std::size_t> images_height;
    ImVec4 label_bg { ImVec4(0, 0, 0, 0) };
};

struct VudiplotColors {
    const ImVec4 midnight_blue
        = ImVec4(0 / 255.f, 1 / 255.f, 36 / 255.f, 240 / 255.f);
    const ImVec4 mild_sand
        = ImVec4(186 / 255.0f, 189 / 255.0f, 113 / 255.0f, 1.0f);
    const ImVec4 red = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    const ImVec4 green = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
    const ImVec4 blue = ImVec4(0.0f, 0.0f, 1.0f, 1.0f);
    const ImVec4 purple = ImVec4(1.0f, 0.0f, 1.0f, 1.0f);
    const ImVec4 black = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    const ImVec4 white = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    const ImVec4 forest_green
        = ImVec4(3 / 255.f, 18 / 255.f, 4 / 255.f, 240 / 255.f);
    const ImVec4 dark_purple
        = ImVec4(22 / 255.f, 1 / 255.f, 24 / 255.f, 240 / 255.f);
    const ImVec4 deep_red
        = ImVec4(24 / 255.f, 1 / 255.f, 8 / 255.f, 240 / 255.f);
};
}
