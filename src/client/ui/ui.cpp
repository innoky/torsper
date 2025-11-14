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

#include "client/ui/ui.hpp"
#include "client/config.hpp"
#include <ftxui/dom/elements.hpp>
#include <vector>
#include <string>

using namespace ftxui;

Element cyber_banner() {
    return vbox({
        text("╔════════════════════════════════════════════════════════════════╗") | color(Color::Red) | bold,
        text("║   ████████╗ ██████╗ ██████╗ ███████╗██████╗ ███████╗██████╗    ║") | color(Color::Yellow) | bold,
        text("║   ╚══██╔══╝██╔═══██╗██╔══██╗██╔════╝██╔══██╗██╔════╝██╔══██╗   ║") | color(Color::Red) | bold,
        text("║      ██║   ██║   ██║██████╔╝███████╗██████╔╝█████╗  ██████╔╝   ║") | color(Color::Yellow) | bold,
        text("║      ██║   ██║   ██║██╔══██╗╚════██║██╔═══╝ ██╔══╝  ██╔══██╗   ║") | color(Color::Red) | bold,
        text("║      ██║   ╚██████╔╝██║  ██║███████║██║     ███████╗██║  ██║   ║") | color(Color::Yellow) | bold,
        text("║      ╚═╝    ╚═════╝ ╚═╝  ╚═╝╚══════╝╚═╝     ╚══════╝╚═╝  ╚═╝   ║") | color(Color::Red) | bold,
        text("╚════════════════════════════════════════════════════════════════╝") | color(Color::Yellow) | bold,
        hbox({
            text("       ") ,
            text("ANONYMOUS ") | color(Color::Red) | bold,
            text("FEED") | color(Color::Yellow) | bold,
            filler(),
            text("v1.0.0") | color(Color::Yellow) | dim
        })
    }) | center | border;
}

Element loading_screen(int progress, int frame) {
    int bar_width = 42;
    int filled = (progress * bar_width) / 100;
    std::string bar = "[";
    for (int i = 0; i < bar_width; ++i) {
        if (i < filled) bar += "█";
        else if (i == filled) bar += "▌";
        else bar += "░";
    }
    bar += "]";
    std::vector<std::string> spinner = {"⠁","⠂","⠄","⡀","⢀","⠠","⠐","⠈"};
    int spinner_idx = (frame / 3) % spinner.size();

    return vbox({
        cyber_banner(),
        text("") | size(HEIGHT, EQUAL, 1),
        hbox({
            text(spinner[spinner_idx]) | color(Color::Red) | bold,
            text(" Connecting to TOR network...") | color(Color::White),
        }) | center,
        text("") | size(HEIGHT, EQUAL, 1),
        text(bar) | color(Color::GreenLight) | center,
        text(std::to_string(progress) + "%") | color(Color::Yellow) | center,
        text("") | size(HEIGHT, EQUAL, 1),
        hbox({
            text("🔐 ") | color(Color::Yellow),
            text("Secure tunnel via SOCKS5") | color(Color::White),
            filler(),
            text("• Nodes: ") | color(Color::Red),
            text(std::to_string(pioneers.size())) | color(Color::GreenLight)
        }) | center
    }) | border | borderStyled(ROUNDED);
}

Element post_card(const std::string& post, int index) {
    Color card_color = (index % 2 == 0) ? Color::Red : Color::Yellow;
    std::string header = "Anonymous #" + std::to_string(index + 1);
    return vbox({
        hbox({
            text("● ") | color(card_color) | bold,
            text(header) | color(Color::Yellow) | bold,
            filler(),
            text("[" + std::to_string(index + 1) + "]") | color(Color::GreenLight) | dim
        }),
        separator(),
        text(post) | color(Color::White)
    }) | border | borderStyled(ROUNDED);
}
