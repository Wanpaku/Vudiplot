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

#include "GenGettext.hpp"

#include <libintl.h>
#include <locale.h>
#include <iostream>

GenGettext::GenGettext(const std::string& resources_dir)
    : resources_dir(resources_dir)
{
}

auto GenGettext::init() -> bool
{
    const char* is_setlocale { setlocale(LC_ALL, "") };
    if (is_setlocale == nullptr) {
        std::cerr << gettext("Setting locale failed!") << "\n";
        return false;
    }
    const std::string locale_path = resources_dir + "/locales";
    const char* is_bindtextdomain { bindtextdomain("vudiplot",
                                                   locale_path.c_str()) };
    if (is_bindtextdomain == nullptr) {
        std::cerr << gettext("Binding textdomain failed!") << "\n";
        return false;
    }

    const char* is_textdomain { textdomain("vudiplot") };
    if (is_textdomain == nullptr) {
        std::cout << gettext("Textdomain failed!") << "\n";
        return false;
    }

    return true;
}
