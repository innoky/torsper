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
#include <filesystem>
#include <string>
#include <iostream>

namespace fs = std::filesystem;

namespace dir_utils
{
    void ensure_data_dir(const std::string& data_dir)
    {
        try
        {
            if (fs::create_directories(data_dir))
            {
                std::cout << "Created directory: " << data_dir << "\n";
            }
            else
            {
                std::cout << "Directory already exists: " << data_dir << "\n";
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to create directory '" << data_dir
                      << "': " << e.what() << "\n";
        }
    }
};
