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

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <curl/curl.h>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <filesystem>
#include <vector>
#include <atomic>

#include "client/config.hpp"
#include "utils/logging/logging.hpp"
#include "client/pionniers/pionniers.hpp"
#include "client/network/network.hpp"
#include "client/ui/ui.hpp"
#include "client/utils/gate_parser.hpp"
#include "utils/base64.hpp"
#include "utils/tor/tor_launcher.hpp"

#pragma comment(lib, "ws2_32.lib")

namespace fs = std::filesystem;
using namespace ftxui;

// Global state definitions
std::vector<std::string> gates;
std::vector<std::string> posts_cache;
std::vector<std::string> pioneers;
std::string pioneers_source = "default";
std::atomic<bool> tor_ready{false};
std::atomic<int> loading_progress{0};
std::atomic<Page> current_page{PAGE_GATE_INPUT};

int main(int argc, char* argv[]) {
    try {
        if (!fs::exists(Config::DATA_DIR)) {
            fs::create_directory(Config::DATA_DIR);
        }

        gates = GatesParser::loadFromFile(Config::GATES_FILE);
        
        if (gates.empty()) {
            current_page = PAGE_GATE_INPUT;
        } else {
            current_page = PAGE_LOADING;
        }

        if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
            std::cerr << "curl_global_init failed\n";
            return 1;
        }

        // Load pioneers from file
        auto file_p = load_pioneers_file();
        if (!file_p.empty()) {
            std::lock_guard<std::recursive_mutex> lg(pioneers_mutex);
            for (auto &p : file_p) {
                if (std::find(pioneers.begin(), pioneers.end(), p) == pioneers.end())
                    pioneers.push_back(p);
            }
            pioneers_source = "file";
        }

        if (pioneers.empty()) {
            std::lock_guard<std::recursive_mutex> lg(pioneers_mutex);
            pioneers.push_back(Config::DEFAULT_PIONEER);
            pioneers_source = "default";
            save_pioneers_file();
        }

        // Launch Tor
        fs::path exe_folder = fs::current_path();
        auto screen = ScreenInteractive::Fullscreen();
        std::cout << "Waiting for SOCKS5..." << std::endl;

        TorConfig config("client", 9050);
        TorLauncher tor_launcher(exe_folder, config);

        std::thread tor_thread([&]() {
            try {
                tor_launcher.launch();
                tor_ready = true;
                std::cout << "SOCKS5 proxy ready on port 9050\n";
            } catch (const std::exception& e) {
                std::cerr << "Tor launch failed: " << e.what() << std::endl;
                tor_ready = false;
            }
        });

        std::this_thread::sleep_for(std::chrono::seconds(10));

        std::string base64_input;
        std::string gate_error_message;
        std::string gate_success_message;

        auto gate_input_component = create_gate_input_component(
            &base64_input,
            &gate_error_message,
            &gate_success_message,
            [&]() {
                try {
                    gate_error_message = "";
                    gate_success_message = "";
                    
                    if (base64_input.empty()) {
                        gate_error_message = "Please enter base64 code";
                        return;
                    }
                    
                    auto parsed_gates = GatesParser::parseFromBase64(base64_input);
                    
                    if (parsed_gates.empty()) {
                        gate_error_message = "No valid .onion addresses found";
                        return;
                    }
         
                    GatesParser::saveToFile(parsed_gates, Config::GATES_FILE);
                    
                    gates = parsed_gates;
                    
                    gate_success_message = "Loaded " + std::to_string(gates.size()) + " gate(s)";
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(800));
                    current_page = PAGE_LOADING;
                    screen.PostEvent(Event::Custom);
                    
                } catch (const std::exception& e) {
                    gate_error_message = std::string("Error: ") + e.what();
                }
            },

            [&]() {
                gates.clear();
                gates.push_back(Config::DEFAULT_GATE);
                
                try {
                    GatesParser::saveToFile(gates, Config::GATES_FILE);
                } catch (...) {}
                
                current_page = PAGE_LOADING;
                screen.PostEvent(Event::Custom);
            }
        );

        int selected = 0;
        std::string input_text;
        std::string status_msg;
        bool loading = false;
        std::string pioneers_export_b64;

        auto menu_entries = std::vector<std::string>{
            "📜 View Posts",
            "✏️  New Post",
            "🔄 Refresh",
            "🌐 Pioneers",
            "🚪 Exit"
        };

        MenuOption menu_option;
        menu_option.on_enter = [&] {
            if (selected == 0) {
                loading = true;
                screen.PostEvent(Event::Custom);
                std::thread([&]() {
                    if (fetch_posts()) {
                        status_msg = "✓ Posts loaded successfully";
                    } else {
                        status_msg = "✗ Failed to load posts";
                    }
                    loading = false;
                    screen.PostEvent(Event::Custom);
                }).detach();
            } else if (selected == 1) {
                current_page = PAGE_NEW_POST;
                input_text.clear();
            } else if (selected == 2) {
                loading = true;
                screen.PostEvent(Event::Custom);
                std::thread([&]() {
                    if (fetch_posts()) {
                        status_msg = "✓ Feed refreshed";
                    } else {
                        status_msg = "✗ Refresh failed";
                    }
                    loading = false;
                    screen.PostEvent(Event::Custom);
                }).detach();
            } else if (selected == 3) {
                current_page = PAGE_PIONEERS;
                {
                    std::lock_guard<std::recursive_mutex> lg(pioneers_mutex);
                    std::string json = "[";
                    for (size_t i = 0; i < pioneers.size(); ++i) {
                        if (i) json += ",";
                        json += "\"" + pioneers[i] + "\"";
                    }
                    json += "]";
                    std::vector<unsigned char> v(json.begin(), json.end());
                    pioneers_export_b64 = base64::encode(v);
                }
                screen.PostEvent(Event::Custom);
            } else if (selected == 4) {
                screen.ExitLoopClosure()();
            }
        };

        auto menu = Menu(&menu_entries, &selected, menu_option);
        auto input_component = Input(&input_text, "Type your anonymous message...");
        auto main_component = Container::Vertical({menu, input_component, gate_input_component});

        int animation_frame = 0;
        auto renderer = Renderer(main_component, [&] {
            animation_frame++;

            // GATE INPUT PAGE
            if (current_page == PAGE_GATE_INPUT) {
                return gate_input_component->Render();
            }

            // LOADING PAGE
            if (current_page == PAGE_LOADING) {
                return loading_screen(loading_progress.load(), animation_frame) | center;
            }

            // PIONEERS PAGE
            if (current_page == PAGE_PIONEERS) {
                Elements list;
                {
                    std::lock_guard<std::recursive_mutex> lg(pioneers_mutex);
                    if (pioneers.empty()) {
                        list.push_back(text("No pioneers available") | color(Color::Yellow) | center);
                    } else {
                        int idx = 1;
                        for (const auto &p : pioneers) {
                            list.push_back(hbox({
                                text(std::to_string(idx++) + ". ") | color(Color::Red),
                                text(p) | color(Color::GreenLight)
                            }));
                        }
                    }
                }
                return vbox({
                    cyber_banner(),
                    separator(),
                    text("🌐 PIONEERS") | color(Color::Yellow) | bold | center,
                    text("") | size(HEIGHT, EQUAL, 1),
                    hbox({
                        vbox(list) | frame | size(HEIGHT, LESS_THAN, 12) | flex,
                        separator(),
                        vbox({
                            text("Source:") | color(Color::Red) | bold,
                            text(pioneers_source) | color(Color::GreenLight),
                            text(""),
                            text("Export (base64):") | color(Color::Red) | bold,
                            text(pioneers_export_b64.empty() ? "(none)" : pioneers_export_b64) | color(Color::Yellow) | border,
                            text(""),
                            text("[E] Export to file") | color(Color::Red),
                            text("[D] Delete all") | color(Color::Red),
                            text("[Esc] Back") | color(Color::Red)
                        }) | size(WIDTH, EQUAL, 60)
                    }) | flex,
                    filler()
                }) | border;
            }

            // NEW POST PAGE
            if (current_page == PAGE_NEW_POST) {
                return vbox({
                    cyber_banner(),
                    separator(),
                    text("CREATE NEW ANONYMOUS POST") | color(Color::Yellow) | bold | center,
                    text("") | size(HEIGHT, EQUAL, 1),
                    input_component->Render() | border | size(WIDTH, EQUAL, 80) | center,
                    text("") | size(HEIGHT, EQUAL, 1),
                    hbox({
                        text("[Enter] Publish") | color(Color::GreenLight),
                        text("  |  "),
                        text("[Esc] Back") | color(Color::Red)
                    }) | center,
                    filler(),
                    separator(),
                    hbox({
                        text("⚡ Status: ") | color(Color::Red),
                        text(status_msg.empty() ? "Ready" : status_msg) |
                            color(status_msg.find("✓") != std::string::npos ? Color::GreenLight : Color::Red),
                        filler(),
                        text("🌐 Nodes: " + std::to_string(pioneers.size())) | color(Color::GreenLight) | dim
                    })
                }) | border;
            }

            // MAIN PAGE
            Elements posts_ui;
            if (loading) {
                posts_ui.push_back(hbox({
                    text("⌛") | color(Color::Red) | bold,
                    text(" Processing...") | color(Color::Yellow)
                }) | center);
            } else if (posts_cache.empty()) {
                posts_ui.push_back(text("No posts yet. Be the first to post!") | color(Color::GreenLight) | center);
            } else {
                for (size_t i = 0; i < posts_cache.size(); ++i) {
                    posts_ui.push_back(post_card(posts_cache[i], i));
                }
            }

            return vbox({
                cyber_banner(),
                separator(),
                hbox({
                    vbox({
                        text("╔═ MENU ═╗") | color(Color::Red) | bold,
                        menu->Render() | border | size(WIDTH, EQUAL, 28)
                    }),
                    separator(),
                    vbox({
                        text("╔═ FEED ═╗") | color(Color::Yellow) | bold,
                        vbox(posts_ui) | vscroll_indicator | frame | size(HEIGHT, GREATER_THAN, 12) | flex
                    }) | flex
                }),
                separator(),
                hbox({
                    text("⚡ Status: ") | color(Color::Red),
                    text(status_msg.empty() ? "Ready" : status_msg) |
                        color(status_msg.find("✓") != std::string::npos ? Color::GreenLight : Color::Red),
                    filler(),
                    text("🌐 Pioneers: " + std::to_string(pioneers.size())) | color(Color::GreenLight) | dim
                })
            }) | border;
        });

        renderer |= CatchEvent([&](Event event) {
            if (event == Event::Character('q') || event == Event::Character('Q')) {
                if (current_page != PAGE_GATE_INPUT) {
                    screen.ExitLoopClosure()();
                    return true;
                }
            }
            
            if (current_page == PAGE_PIONEERS) {
                if (event == Event::Escape) {
                    current_page = PAGE_MAIN;
                    return true;
                }
                if (event == Event::Character('e') || event == Event::Character('E')) {
                    std::ofstream out(Config::DATA_DIR + "/pioneers_export.b64", std::ios::trunc);
                    out << pioneers_export_b64;
                    out.close();
                    return true;
                }
                if (event == Event::Character('d') || event == Event::Character('D')) {
                    {
                        std::lock_guard<std::recursive_mutex> lg(pioneers_mutex);
                        pioneers.clear();
                        pioneers.push_back(Config::DEFAULT_PIONEER);
                        pioneers_source = "default";
                        save_pioneers_file();
                    }
                    current_page = PAGE_MAIN;
                    return true;
                }
            }

            if (current_page == PAGE_NEW_POST) {
                if (event == Event::Return && !input_text.empty()) {
                    loading = true;
                    std::string post_copy = input_text;
                    current_page = PAGE_MAIN;
                    std::thread([&, post_copy]() {
                        if (send_post_to_all(post_copy)) {
                            status_msg = "✓ Post published to TORSPER";
                            fetch_posts();
                        } else {
                            status_msg = "✗ Failed to publish post";
                        }
                        loading = false;
                        screen.PostEvent(Event::Custom);
                    }).detach();
                    return true;
                } else if (event == Event::Escape) {
                    current_page = PAGE_MAIN;
                    return true;
                }
            }
            return false;
        });

        std::thread loading_thread([&]() {
            while (current_page.load() == PAGE_GATE_INPUT) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                screen.PostEvent(Event::Custom);
            }

            auto start = std::chrono::steady_clock::now();

            while (true) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start
                ).count();
                bool min_time = elapsed >= 2000;
                if (min_time && tor_ready.load()) {
                    std::thread([&]() {
                        std::cerr << "[INFO] Updating pioneers list from gates...\n";
                        if (update_pioneers_from_gates()) {
                            std::cerr << "[SUCCESS] Pioneers updated successfully\n";
                        } else {
                            std::cerr << "[INFO] Using existing pioneers list\n";
                        }
                        screen.PostEvent(Event::Custom);
                    }).detach();

                    std::this_thread::sleep_for(std::chrono::milliseconds(300));
                    current_page = PAGE_MAIN;
                    screen.PostEvent(Event::Custom);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                screen.PostEvent(Event::Custom);
            }
        });

        screen.Loop(renderer);

        if (tor_thread.joinable()) tor_thread.join();
        curl_global_cleanup();

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}