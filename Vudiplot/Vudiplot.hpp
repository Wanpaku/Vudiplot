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

#include <memory>
#include <InitUtils.hpp>

template <std::derived_from<VudiInit::PlotInit> T> class Vudiplot {

public:
    Vudiplot();
    ~Vudiplot();

    Vudiplot(const Vudiplot<T>& src) = delete;
    Vudiplot<T>& operator=(const Vudiplot<T>& src) = delete;

    Vudiplot(Vudiplot<T>&& rhs) noexcept;
    Vudiplot<T>& operator=(Vudiplot<T>&& rhs) noexcept;

    [[nodiscard]] auto set_plot_params(const T& plot_config) -> bool;

    void show();

    void clean_up_resources();

private:
    class VudiImpl;
    std::unique_ptr<VudiImpl> vudi_implt { nullptr };
};
