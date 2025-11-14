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
#include <mutex>

extern std::recursive_mutex pioneers_mutex;

// Parse pioneers from various formats
std::vector<std::string> parse_pioneers_from_string(const std::string& input);
std::vector<std::string> parse_lines(const std::string& input);

// File operations
bool save_pioneers_file();
std::vector<std::string> load_pioneers_file();

// Load from command line arguments
void load_gates_from_argv(int argc, char* argv[]);

// Update pioneers from gates
bool update_pioneers_from_gates();
