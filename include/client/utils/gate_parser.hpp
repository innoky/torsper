/*
 * Copyright (C) 2025 Anatoly Nikolaevich
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <filesystem>
#include "utils/base64.hpp"

namespace fs = std::filesystem;

class GatesParser {
public:
    static std::vector<std::string> parseFromBase64(const std::string& base64_input) {
        std::vector<std::string> gates;
        
        try {
            std::vector<unsigned char> decoded_bytes = base64::decode(base64_input);
            
            std::string decoded(decoded_bytes.begin(), decoded_bytes.end());
            
            std::istringstream stream(decoded);
            std::string line;
            
            while (std::getline(stream, line)) {
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);
                
                if (!line.empty() && line.find(".onion") != std::string::npos) {
                    gates.push_back(line);
                }
            }
            
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to parse gates from base64: " + std::string(e.what()));
        }
        
        return gates;
    }

    static std::string encodeToBase64(const std::vector<std::string>& gates) {
        std::string combined;
        for (size_t i = 0; i < gates.size(); ++i) {
            combined += gates[i];
            if (i < gates.size() - 1) {
                combined += "\n";
            }
        }

        std::vector<unsigned char> bytes(combined.begin(), combined.end());
        
        return base64::encode(bytes);
    }

    static void saveToFile(const std::vector<std::string>& gates, const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file for writing: " + filepath);
        }
        
        for (const auto& gate : gates) {
            file << gate << "\n";
        }
        
        file.close();
    }

   
    static std::vector<std::string> loadFromFile(const std::string& filepath) {
        std::vector<std::string> gates;
        
        if (!fs::exists(filepath)) {
            return gates;
        }
        
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return gates;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            
            if (!line.empty() && line.find(".onion") != std::string::npos) {
                gates.push_back(line);
            }
        }
        
        file.close();
        return gates;
    }
};