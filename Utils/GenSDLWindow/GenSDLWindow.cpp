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

#include "GenSDLWindow.hpp"
#include <libintl.h>
#include <locale.h>
#include <stdexcept>

GenSDLWindow::GenSDLWindow()
{
}

GenSDLWindow::~GenSDLWindow()
{
    clean_up_window();
}

void GenSDLWindow::clean_up_window()
{
    if (sdl_window) {
        SDL_DestroyWindow(sdl_window);
        sdl_window = nullptr;
    }
    SDL_Quit();
}

void GenSDLWindow::init_sdl_window(const std::string& window_name,
                                   const uint32_t width, const uint32_t height)
{
    // SDL3 window initialization
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log(gettext("SDL_Init failed: %s"), SDL_GetError());
        throw std::runtime_error(gettext("SDL window initialization failed."));
    }

    sdl_window = SDL_CreateWindow(window_name.c_str(), /* title */
                                  width, height, /* width, height */
                                  SDL_WINDOW_RESIZABLE);

    if (!sdl_window) {
        SDL_Log(gettext("SDL_CreateWindow failed: %s\n"), SDL_GetError());
        SDL_Quit();
        throw std::runtime_error(gettext("Creation of SDL window failed."));
    }
    SDL_SyncWindow(sdl_window);
}

auto GenSDLWindow::get_sdl_window() -> SDL_Window*
{
    return sdl_window;
}
