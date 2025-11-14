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

#ifndef TOR_LAUNCHER_HPP
#define TOR_LAUNCHER_HPP

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>
#include <optional>
#include <sstream>

namespace fs = std::filesystem;

enum class TorMode {
    CLIENT_ONLY,
    HIDDEN_SERVICE
};

struct TorConfig {
    std::string name;
    int socks_port;
    TorMode mode;
    std::optional<int> hidden_service_port;

    TorConfig(const std::string& n, int sp)
        : name(n), socks_port(sp), mode(TorMode::CLIENT_ONLY), hidden_service_port(std::nullopt) {}

    TorConfig(const std::string& n, int sp, int hs_port)
        : name(n), socks_port(sp), mode(TorMode::HIDDEN_SERVICE), hidden_service_port(hs_port) {}
};

class TorLauncher {
private:
    PROCESS_INFORMATION process_info_;
    std::string onion_address_;
    TorConfig config_;
    fs::path exe_folder_;

    fs::path get_data_dir() const {
        return exe_folder_ / "data" / config_.name;
    }

    fs::path get_torrc_path() const {
        return get_data_dir() / ("torrc_" + config_.name);
    }

    fs::path get_hidden_dir() const {
        return get_data_dir() / "hidden_service";
    }

    fs::path get_tor_data_dir() const {
        return get_data_dir() / ("tor_data_" + config_.name);
    }

    void create_directories() {
        fs::create_directories(get_tor_data_dir());
        if (config_.mode == TorMode::HIDDEN_SERVICE) {
            fs::create_directories(get_hidden_dir());
        }
    }

    void create_torrc() {
        fs::path torrc_path = get_torrc_path();
        fs::path data_dir = get_data_dir();

        std::ofstream torrc(torrc_path);
        if (!torrc) {
            throw std::runtime_error("Failed to create torrc file: " + torrc_path.string());
        }

        torrc << "SocksPort " << config_.socks_port << "\n";
        torrc << "DataDirectory " << get_tor_data_dir().string() << "\n";

        if (config_.mode == TorMode::HIDDEN_SERVICE && config_.hidden_service_port.has_value()) {
            torrc << "HiddenServiceDir " << get_hidden_dir().string() << "\n";
            torrc << "HiddenServicePort 80 127.0.0.1:" << config_.hidden_service_port.value() << "\n";
        }

        torrc << "Log notice file " << (data_dir / "tor.log").string() << "\n";
        torrc << "Log notice stdout\n";  
    }

    bool is_process_running() const {
        if (!process_info_.hProcess) return false;

        DWORD exit_code;
        if (!GetExitCodeProcess(process_info_.hProcess, &exit_code)) {
            return false;
        }
        return exit_code == STILL_ACTIVE;
    }

    std::string read_tor_log() const {
        fs::path log_path = get_data_dir() / "tor.log";
        if (!fs::exists(log_path)) return "";

        std::ifstream log_file(log_path);
        std::stringstream buffer;
        buffer << log_file.rdbuf();
        return buffer.str();
    }

    void wait_for_hostname() {
        if (config_.mode != TorMode::HIDDEN_SERVICE) {
            return;
        }

        fs::path hostname_path = get_hidden_dir() / "hostname";

      
        for (int i = 0; i < 120; ++i) {
            if (!is_process_running()) {
                std::string log_content = read_tor_log();
                throw std::runtime_error(
                    "Tor process died during startup.\nLast log entries:\n" +
                    log_content.substr(log_content.length() > 1000 ? log_content.length() - 1000 : 0)
                );
            }

          
            if (fs::exists(hostname_path)) {
                std::ifstream hostfile(hostname_path);
                if (hostfile && std::getline(hostfile, onion_address_)) {
                  
                    onion_address_.erase(
                        std::remove_if(onion_address_.begin(), onion_address_.end(), ::isspace),
                        onion_address_.end()
                    );

                    if (!onion_address_.empty()) {
                        return;
                    }
                }
            }

            Sleep(500); 
        }

        std::string log_content = read_tor_log();
        throw std::runtime_error(
            "Tor started, but onion hostname not found after 60 seconds.\nTor log:\n" +
            log_content.substr(log_content.length() > 1000 ? log_content.length() - 1000 : 0)
        );
    }

    void wait_for_tor_bootstrap() {
       
        fs::path log_path = get_data_dir() / "tor.log";

        for (int i = 0; i < 60; ++i) {  
            if (!is_process_running()) {
                throw std::runtime_error("Tor process died during bootstrap");
            }

            if (fs::exists(log_path)) {
                std::ifstream log_file(log_path);
                std::string log_content((std::istreambuf_iterator<char>(log_file)),
                                       std::istreambuf_iterator<char>());

             
                if (log_content.find("Bootstrapped 100%") != std::string::npos ||
                    log_content.find("100% (done): Done") != std::string::npos) {
                    return;
                }

     
                if (log_content.find("[err]") != std::string::npos ||
                    log_content.find("[warn] Problem bootstrapping") != std::string::npos) {
                    throw std::runtime_error("Tor bootstrap error. Check log: " + log_path.string());
                }
            }

            Sleep(500);
        }
    }

public:
    TorLauncher(const fs::path& exe_folder, const TorConfig& config)
        : exe_folder_(exe_folder), config_(config) {
        process_info_ = {};
    }

    ~TorLauncher() {
        stop();
    }

    std::string launch() {
        fs::path tor_path = exe_folder_ / "tor" / "tor.exe";

        if (!fs::exists(tor_path)) {
            throw std::runtime_error("Tor executable not found: " + tor_path.string());
        }

        fs::create_directories(get_data_dir());
        create_directories();
        create_torrc();

        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        std::string cmd = "\"" + tor_path.string() + "\" -f \"" + get_torrc_path().string() + "\"";
        std::vector<char> cmd_buf(cmd.begin(), cmd.end());
        cmd_buf.push_back('\0');

        process_info_ = {};
        if (!CreateProcessA(
                nullptr,
                cmd_buf.data(),
                nullptr,
                nullptr,
                FALSE,
                CREATE_NO_WINDOW,
                nullptr,
                nullptr,
                &si,
                &process_info_))
        {
            DWORD err = GetLastError();
            LPSTR msgBuf = nullptr;
            FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPSTR)&msgBuf, 0, nullptr);

            std::string errmsg = "Failed to launch Tor. WinErr=" + std::to_string(err);
            if (msgBuf) {
                errmsg += ": " + std::string(msgBuf);
                LocalFree(msgBuf);
            }
            throw std::runtime_error(errmsg);
        }

       
        Sleep(2000);
       
        if (!is_process_running()) {
            throw std::runtime_error("Tor process failed to start");
        }

      
        wait_for_tor_bootstrap();
        
        wait_for_hostname();

        return onion_address_;
    }

    void stop() {
        if (process_info_.hProcess) {
            TerminateProcess(process_info_.hProcess, 0);
            WaitForSingleObject(process_info_.hProcess, 5000);

            CloseHandle(process_info_.hProcess);
            process_info_.hProcess = NULL;

            if (process_info_.hThread) {
                CloseHandle(process_info_.hThread);
                process_info_.hThread = NULL;
            }

            process_info_.dwProcessId = 0;
            process_info_.dwThreadId = 0;
        }
    }

    const std::string& get_onion_address() const {
        return onion_address_;
    }

    const TorConfig& get_config() const {
        return config_;
    }

    PROCESS_INFORMATION get_process_info() const {
        return process_info_;
    }

    bool is_hidden_service() const {
        return config_.mode == TorMode::HIDDEN_SERVICE;
    }
};

#endif 
