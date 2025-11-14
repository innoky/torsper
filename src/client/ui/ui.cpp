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
#include <ftxui/component/component.hpp>
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

Element gate_input_banner() {
    return vbox({
        text("╔════════════════════════════════════════════════════════════════╗") | color(Color::Red) | bold,
        text("║   ███████╗███████╗████████╗██╗   ██╗██████╗                    ║") | color(Color::Yellow) | bold,
        text("║   ██╔════╝██╔════╝╚══██╔══╝██║   ██║██╔══██╗                   ║") | color(Color::Red) | bold,
        text("║   ███████╗█████╗     ██║   ██║   ██║██████╔╝                   ║") | color(Color::Yellow) | bold,
        text("║   ╚════██║██╔══╝     ██║   ██║   ██║██╔═══╝                    ║") | color(Color::Red) | bold,
        text("║   ███████║███████╗   ██║   ╚██████╔╝██║                        ║") | color(Color::Yellow) | bold,
        text("║   ╚══════╝╚══════╝   ╚═╝    ╚═════╝ ╚═╝                        ║") | color(Color::Red) | bold,
        text("╚════════════════════════════════════════════════════════════════╝") | color(Color::Yellow) | bold,
        hbox({
            text("       ") ,
            text("GATE ") | color(Color::Red) | bold,
            text("CONFIGURATION") | color(Color::Yellow) | bold,
            filler(),
            text("STEP 1/2") | color(Color::Yellow) | dim
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

Component create_gate_input_component(
    std::string* base64_input,
    std::string* error_message,
    std::string* success_message,
    std::function<void()> on_decode,
    std::function<void()> on_skip
) {
    InputOption input_opts;
    input_opts.multiline = false;
    auto input_component = Input(base64_input, "Paste base64 encoded gates here...", input_opts);
    
    auto decode_button = Button("🔓 Decode & Continue", on_decode);
    auto skip_button = Button("⏭ Skip (Use Default)", on_skip);
    
    auto button_container = Container::Horizontal({
        decode_button,
        skip_button
    });
    
    auto main_container = Container::Vertical({
        input_component,
        button_container
    });
    
    return Renderer(main_container, [=] {
        return vbox({
            gate_input_banner(),
            text("") | size(HEIGHT, EQUAL, 1),
            separator(),
            text("") | size(HEIGHT, EQUAL, 1),
            
            vbox({
                hbox({
                    text("🔑 ") | color(Color::Yellow) | bold,
                    text("Enter base64 encoded list of gates") | color(Color::White) | bold
                }),
                text("   Each gate address should be on a new line, ending with .onion") | color(Color::GrayLight),
                text("   Use Tab to switch between input and buttons") | color(Color::GrayLight) | dim,
                text("") | size(HEIGHT, EQUAL, 1),
            }) | center,
            
            hbox({
                text("   "),
                vbox({
                    input_component->Render() | size(WIDTH, GREATER_THAN, 60) | size(HEIGHT, EQUAL, 3) | border | borderStyled(ROUNDED),
                    text("") | size(HEIGHT, EQUAL, 1),
                    hbox({
                        decode_button->Render(),
                        text("  "),
                        skip_button->Render()
                    }) | center
                }) | flex
            }) | flex,
            
            text("") | size(HEIGHT, EQUAL, 1),
            
            vbox({
                error_message->empty() ? text("") : 
                    hbox({
                        text("   ❌ ") | color(Color::Red) | bold,
                        text(*error_message) | color(Color::Red) | bold
                    }),
                success_message->empty() ? text("") : 
                    hbox({
                        text("   ✓ ") | color(Color::GreenLight) | bold,
                        text(*success_message) | color(Color::GreenLight) | bold
                    })
            }),
            
            filler(),
            separator(),
            
            vbox({
                hbox({
                    text("💡 ") | color(Color::Yellow),
                    text("Tip: You can skip this step to use the default gate") | color(Color::GrayLight)
                }) | center,
                hbox({
                    text("🔐 ") | color(Color::Red),
                    text("Your gates will be saved locally and loaded automatically") | color(Color::GrayLight)
                }) | center,
                hbox({
                    text("⌨️  ") | color(Color::Cyan),
                    text("Press Tab to navigate, Enter to activate buttons") | color(Color::GrayLight)
                }) | center
            })
        }) | border | borderStyled(ROUNDED);
    });
}