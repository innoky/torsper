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
#include <string>
#include <vector>
#include <atomic>

// Константы
namespace Config {
    static const std::string DATA_DIR = "data";
    static const std::string PIONEERS_FILE = "data/pioneers.json";
    static const std::string GATES_FILE = "data/gates.txt";
    static const std::string DEFAULT_GATE = "3oncms4bmvcv6jvwgzjvovfuhlx6pdho26lo6jny3ruu3hpgz7belzqd.onion";
    static const std::string DEFAULT_PIONEER = "5krka4isaabbpp7fbs3rqacryhvzxpx2b6sirabhbo73bolfbjs5yrqd.onion";
}

// Page enum
enum Page {
    PAGE_GATE_INPUT,
    PAGE_LOADING,
    PAGE_MAIN,
    PAGE_NEW_POST,
    PAGE_PIONEERS
};

// Global state (extern declarations)
extern std::vector<std::string> gates;
extern std::vector<std::string> posts_cache;
extern std::vector<std::string> pioneers;
extern std::string pioneers_source;
extern std::atomic<bool> tor_ready;
extern std::atomic<int> loading_progress;
extern std::atomic<Page> current_page;