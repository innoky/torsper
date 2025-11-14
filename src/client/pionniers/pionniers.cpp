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

#include "utils/base64.hpp"
#include <fstream>
#include <algorithm>
#include <sstream>
#include <filesystem>
#include <iostream>

#include "client/pionniers/pionniers.hpp"
#include "client/config.hpp"
#include "client/network/network.hpp"


namespace fs = std::filesystem;

std::recursive_mutex pioneers_mutex;

bool ensure_data_dir(std::string data_dir)
{
    try {
       
        // Создаем директорию, если она не существует
        if (!fs::exists(data_dir)) {
            return fs::create_directories(data_dir);
        }
        
      
        if (!fs::is_directory(data_dir)) {
            std::cerr << "[ERROR] Data path is not a directory: " << data_dir << std::endl;
            return false;
        }
        
     
        std::string test_file = data_dir + "/write_test.tmp";
        std::ofstream test(test_file);
        if (!test) {
            std::cerr << "[ERROR] No write permission in data directory: " << data_dir << std::endl;
            return false;
        }
        test.close();
        fs::remove(test_file);
        
        return true;
    } catch (const fs::filesystem_error& ex) {
        std::cerr << "[ERROR] Filesystem error ensuring data directory: " << ex.what() << std::endl;
        return false;
    } catch (const std::exception& ex) {
        std::cerr << "[ERROR] Error ensuring data directory: " << ex.what() << std::endl;
        return false;
    }
}

std::vector<std::string> parse_lines(const std::string& input) {
    std::vector<std::string> out;
    std::stringstream ss(input);
    std::string line;
    while (std::getline(ss, line)) {
        // trim
        line.erase(line.begin(), std::find_if(line.begin(), line.end(),
            [](unsigned char ch){ return !std::isspace(ch); }));
        line.erase(std::find_if(line.rbegin(), line.rend(),
            [](unsigned char ch){ return !std::isspace(ch); }).base(), line.end());
        if (!line.empty() && line.find(".onion") != std::string::npos) {
            out.push_back(line);
        }
    }
    return out;
}

std::vector<std::string> parse_pioneers_from_string(const std::string& input) {
    std::vector<std::string> out;
    size_t a = input.find('[');
    size_t b = input.rfind(']');
    if (a == std::string::npos || b == std::string::npos || b <= a) return out;
    std::string arr = input.substr(a + 1, b - a - 1);
    size_t pos = 0;
    while (pos < arr.size()) {
        size_t q1 = arr.find('"', pos);
        if (q1 == std::string::npos) break;
        size_t q2 = arr.find('"', q1 + 1);
        if (q2 == std::string::npos) break;
        std::string item = arr.substr(q1 + 1, q2 - q1 - 1);
        // trim
        item.erase(item.begin(), std::find_if(item.begin(), item.end(),
            [](unsigned char ch){ return !std::isspace(ch); }));
        item.erase(std::find_if(item.rbegin(), item.rend(),
            [](unsigned char ch){ return !std::isspace(ch); }).base(), item.end());
        if (!item.empty() && item.find(".onion") != std::string::npos) out.push_back(item);
        pos = q2 + 1;
    }
    return out;
}

bool save_pioneers_file() {
    std::lock_guard<std::recursive_mutex> lg(pioneers_mutex);
    ensure_data_dir(Config::PIONEERS_FILE);

    try {
        std::ofstream out(Config::PIONEERS_FILE, std::ios::trunc);
        if (!out) return false;
        out << "[\n";
        for (size_t i = 0; i < pioneers.size(); ++i) {
            out << "  \"" << pioneers[i] << "\"";
            if (i + 1 < pioneers.size()) out << ",";
            out << "\n";
        }
        out << "]\n";
        out.close();
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> load_pioneers_file() {
    std::vector<std::string> result;
    std::lock_guard<std::recursive_mutex> lg(pioneers_mutex);
    ensure_data_dir(Config::PIONEERS_FILE);
    try {
        std::ifstream in(Config::PIONEERS_FILE);
        std::string all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();
        size_t pos = 0;
        while (true) {
            size_t q1 = all.find('"', pos);
            if (q1 == std::string::npos) break;
            size_t q2 = all.find('"', q1 + 1);
            if (q2 == std::string::npos) break;
            std::string s = all.substr(q1 + 1, q2 - q1 - 1);
            pos = q2 + 1;
            if (s.find(".onion") != std::string::npos) result.push_back(s);
        }
    } catch (...) {}
    return result;
}

void load_gates_from_argv(int argc, char* argv[]) {
    if (argc < 2) return;
    try {
        std::string encoded = argv[1];
        auto decoded_vec = base64::decode(encoded);
        std::string decoded(decoded_vec.begin(), decoded_vec.end());

        auto parsed = parse_lines(decoded);
        if (parsed.empty()) {
            parsed = parse_pioneers_from_string(decoded);
        }

        if (!parsed.empty()) {
            std::lock_guard<std::recursive_mutex> lg(pioneers_mutex);
            for (auto &p : parsed) {
                if (std::find(pioneers.begin(), pioneers.end(), p) == pioneers.end())
                    pioneers.push_back(p);
            }
            pioneers_source = "argv";
            save_pioneers_file();
        }
    } catch (...) {}
}

bool update_pioneers_from_gates() {
    auto new_pioneers = fetch_servers_from_gates();

    if (new_pioneers.empty()) {
        std::cerr << "[WARN] Gates did not return any pioneers\n";
        return false;
    }

    std::lock_guard<std::recursive_mutex> lg(pioneers_mutex);

    for (const auto& p : new_pioneers) {
        if (std::find(pioneers.begin(), pioneers.end(), p) == pioneers.end()) {
            pioneers.push_back(p);
        }
    }

    save_pioneers_file();
    std::cerr << "[INFO] Updated pioneers list, now have "
              << pioneers.size() << " pioneers\n";
    return true;
}
