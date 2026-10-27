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

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <string>

class GenSDLWindow {

public:
    explicit GenSDLWindow();
    ~GenSDLWindow();

    GenSDLWindow(const GenSDLWindow& src) = delete;
    GenSDLWindow& operator=(const GenSDLWindow& src) = delete;
    GenSDLWindow(GenSDLWindow&& rhs) = delete;
    GenSDLWindow& operator=(GenSDLWindow&& rhs) = delete;

    void init_sdl_window(const std::string& window_name, const uint32_t width,
                         const uint32_t height);

    auto get_sdl_window() -> SDL_Window*;

private:
    SDL_Window* sdl_window;

    void clean_up_window();
};
