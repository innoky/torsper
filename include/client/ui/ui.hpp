/*
 * Copyright (C) 2025 Anatoly Nikolaevich
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <string>
#include <functional>

using namespace ftxui;

Element cyber_banner();
Element loading_screen(int progress, int frame);
Element post_card(const std::string& post, int index);
Element gate_input_banner();

Component create_gate_input_component(
    std::string* base64_input,
    std::string* error_message,
    std::string* success_message,
    std::function<void()> on_decode,
    std::function<void()> on_skip
);