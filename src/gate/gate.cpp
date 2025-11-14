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

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <nlohmann/json.hpp>

#include <ftxui/screen/screen.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include <fstream>
#include <sstream>
#include <algorithm>

using json = nlohmann::json;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
using tcp       = net::ip::tcp;
namespace fs    = std::filesystem;
using namespace ftxui;

// ---------------------- Data -------------------------
std::vector<std::string> Pioners = {
    "5krka4isaabbpp7fbs3rqacryhvzxpx2b6sirabhbo73bolfbjs5yrqd.onion"
};

std::atomic<bool> server_running{false};
std::atomic<int> total_requests{0};
std::string onion_address;
std::atomic<bool> tor_ready{false};

// Tor process info
PROCESS_INFORMATION tor_process_info = {};

struct LogEntry {
    std::string timestamp;
    std::string message;
    int type; // 0=info, 1=success, 2=error
};

std::vector<LogEntry> logs;
std::mutex logs_mtx;

void add_log(const std::string& msg, int type = 0) {
    std::lock_guard<std::mutex> lk(logs_mtx);
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&time));

    logs.push_back({std::string(buf), msg, type});
    if (logs.size() > 50) logs.erase(logs.begin());
}

// ---------------------- Tor Launcher -------------------------
class SimpleTorLauncher {
private:
    fs::path exe_folder_;
    std::string service_name_;
    int socks_port_;
    int local_port_;

    fs::path get_data_dir() const {
        return exe_folder_ / "data" / service_name_;
    }

    fs::path get_torrc_path() const {
        return get_data_dir() / ("torrc_" + service_name_);
    }

    fs::path get_hidden_dir() const {
        return get_data_dir() / "hidden_service";
    }

    fs::path get_tor_data_dir() const {
        return get_data_dir() / ("tor_data_" + service_name_);
    }

    fs::path get_log_path() const {
        return get_data_dir() / "tor.log";
    }

    void create_directories() {
        fs::create_directories(get_tor_data_dir());
        fs::create_directories(get_hidden_dir());
    }

    void create_torrc() {
        fs::path torrc_path = get_torrc_path();

        std::ofstream torrc(torrc_path);
        if (!torrc) {
            throw std::runtime_error("Failed to create torrc: " + torrc_path.string());
        }

        torrc << "SocksPort " << socks_port_ << "\n";
        torrc << "DataDirectory " << get_tor_data_dir().string() << "\n";
        torrc << "HiddenServiceDir " << get_hidden_dir().string() << "\n";
        torrc << "HiddenServicePort 80 127.0.0.1:" << local_port_ << "\n";
        torrc << "Log notice file " << get_log_path().string() << "\n";
        torrc << "Log notice stdout\n";
        torrc.close();

        add_log("Created torrc at: " + torrc_path.string(), 0);
    }

    bool is_process_running() const {
        if (!tor_process_info.hProcess) return false;

        DWORD exit_code;
        if (!GetExitCodeProcess(tor_process_info.hProcess, &exit_code)) {
            return false;
        }
        return exit_code == STILL_ACTIVE;
    }

    std::string read_tor_log() const {
        fs::path log_path = get_log_path();
        if (!fs::exists(log_path)) return "";

        std::ifstream log_file(log_path);
        std::stringstream buffer;
        buffer << log_file.rdbuf();
        return buffer.str();
    }

    void wait_for_tor_bootstrap() {
        add_log("Waiting for Tor bootstrap...", 0);
        fs::path log_path = get_log_path();

        for (int i = 0; i < 120; ++i) {  
            if (!is_process_running()) {
                std::string log_content = read_tor_log();
                throw std::runtime_error(
                    "Tor process died during bootstrap. Log:\n" +
                    (log_content.length() > 500 ? log_content.substr(log_content.length() - 500) : log_content)
                );
            }

            if (fs::exists(log_path)) {
                std::string log_content = read_tor_log();

                if (log_content.find("Bootstrapped 100%") != std::string::npos) {
                    add_log("Tor bootstrapped successfully!", 1);
                    return;
                }

                if (log_content.find("[err]") != std::string::npos) {
                    throw std::runtime_error("Tor bootstrap error detected. Check: " + log_path.string());
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        throw std::runtime_error("Tor bootstrap timeout after 60 seconds");
    }

    std::string wait_for_hostname() {
        add_log("Waiting for onion hostname...", 0);
        fs::path hostname_path = get_hidden_dir() / "hostname";

        for (int i = 0; i < 120; ++i) {  
            if (!is_process_running()) {
                throw std::runtime_error("Tor process died while waiting for hostname");
            }

            if (fs::exists(hostname_path)) {
                std::ifstream hostfile(hostname_path);
                std::string hostname;
                if (hostfile && std::getline(hostfile, hostname)) {
                   
                    hostname.erase(
                        std::remove_if(hostname.begin(), hostname.end(), ::isspace),
                        hostname.end()
                    );

                    if (!hostname.empty()) {
                        add_log("Onion hostname ready: " + hostname, 1);
                        return hostname;
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        std::string log_content = read_tor_log();
        throw std::runtime_error(
            "Hostname not found after 60 seconds. Log:\n" +
            (log_content.length() > 500 ? log_content.substr(log_content.length() - 500) : log_content)
        );
    }

public:
    SimpleTorLauncher(const fs::path& exe_folder, const std::string& service_name,
                      int socks_port, int local_port)
        : exe_folder_(exe_folder), service_name_(service_name),
          socks_port_(socks_port), local_port_(local_port) {}

    std::string launch() {
        fs::path tor_path = exe_folder_ / "tor" / "tor.exe";

        if (!fs::exists(tor_path)) {
            throw std::runtime_error("Tor executable not found: " + tor_path.string());
        }

        add_log("Found Tor at: " + tor_path.string(), 0);


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

        add_log("Launching Tor process...", 0);
        add_log("Command: " + cmd, 0);

        tor_process_info = {};
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
                &tor_process_info))
        {
            DWORD err = GetLastError();
            LPSTR msgBuf = nullptr;
            FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPSTR)&msgBuf, 0, nullptr);

            std::string errmsg = "Failed to launch Tor. Error " + std::to_string(err);
            if (msgBuf) {
                errmsg += ": " + std::string(msgBuf);
                LocalFree(msgBuf);
            }
            throw std::runtime_error(errmsg);
        }

        add_log("Tor process started (PID: " + std::to_string(tor_process_info.dwProcessId) + ")", 1);


        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (!is_process_running()) {
            throw std::runtime_error("Tor process failed to start properly");
        }

        wait_for_tor_bootstrap();

        return wait_for_hostname();
    }

    void stop() {
        if (tor_process_info.hProcess) {
            add_log("Stopping Tor process...", 0);
            TerminateProcess(tor_process_info.hProcess, 0);
            WaitForSingleObject(tor_process_info.hProcess, 5002);

            CloseHandle(tor_process_info.hProcess);
            CloseHandle(tor_process_info.hThread);

            tor_process_info = {};
            add_log("Tor stopped", 1);
        }
    }
};

// ---------------------- Server Logic -------------------------
std::string getActivePioners() {
    std::string result;
    for (const auto& p : Pioners) {
        result += p + "\n";
    }
    return result;
}

std::string addPionnier(std::string onion_addr) {
    Pioners.push_back(onion_addr);
    return "Pionnier added successfully";
}

void handle_request(const http::request<http::string_body>& req,
                    http::response<http::string_body>& res)
{
    total_requests++;

    if (req.method() == http::verb::get && req.target() == "/get_pionniers")
    {
        add_log("GET /get_pionniers - Returned " + std::to_string(Pioners.size()) + " pioneers", 1);
        res.result(http::status::ok);
        res.set(http::field::content_type, "text/plain");
        res.body() = getActivePioners();
        res.prepare_payload();
    }
    else if (req.method() == http::verb::post && req.target() == "/add_pionnier")
    {
        try {
            json parsed = json::parse(req.body());

            if (!parsed.contains("onion_address")) {
                throw std::runtime_error("Missing key: onion_address");
            }

            std::string onion_addr = parsed["onion_address"].get<std::string>();
            add_log("POST /add_pionnier - Added: " + onion_addr, 1);

            res.result(http::status::ok);
            res.set(http::field::content_type, "text/plain");
            res.body() = addPionnier(onion_addr);
        }
        catch (const std::exception& e) {
            add_log(std::string("JSON parse error: ") + e.what(), 2);
            res.result(http::status::bad_request);
            res.set(http::field::content_type, "text/plain");
            res.body() = "Invalid JSON";
        }
        res.prepare_payload();
    }
    else {
        add_log("404: " + std::string(req.target()), 2);
        res.result(http::status::not_found);
        res.set(http::field::content_type, "text/plain");
        res.body() = "404 Not Found";
        res.prepare_payload();
    }
}

// ---------------------- UI -------------------------
Element gate_banner() {
    return vbox({
        text("╔═════════════════════════════════════════════╗") | color(Color::Magenta) | bold,
        text("║      ██████╗  █████╗ ████████╗███████╗      ║") | color(Color::MagentaLight) | bold,
        text("║     ██╔════╝ ██╔══██╗╚══██╔══╝██╔════╝      ║") | color(Color::Magenta) | bold,
        text("║     ██║  ███╗███████║   ██║   █████╗        ║") | color(Color::MagentaLight) | bold,
        text("║     ██║   ██║██╔══██║   ██║   ██╔══╝        ║") | color(Color::Magenta) | bold,
        text("║     ╚██████╔╝██║  ██║   ██║   ███████╗      ║") | color(Color::MagentaLight) | bold,
        text("║      ╚═════╝ ╚═╝  ╚═╝   ╚═╝   ╚══════╝      ║") | color(Color::Magenta) | bold,
        text("╚═════════════════════════════════════════════╝") | color(Color::MagentaLight) | bold,
        hbox({
            text("       TORSPER DISCOVERY GATE ") | color(Color::Magenta) | bold,
            filler(),
            text("v1.0") | color(Color::MagentaLight) | dim
        })
    }) | center | border;
}

Element pioneers_box() {
    Elements pioneer_list;
    int idx = 1;
    for (const auto& p : Pioners) {
        pioneer_list.push_back(
            hbox({
                text(std::to_string(idx++) + ". ") | color(Color::Yellow),
                text(p) | color(Color::GreenLight)
            })
        );
    }

    return vbox({
        text("🌐 REGISTERED PIONNIERS") | color(Color::Yellow) | bold | center,
        separator(),
        vbox(pioneer_list) | vscroll_indicator | frame | flex,
        separator(),
        hbox({
            text("Total: ") | color(Color::White),
            text(std::to_string(Pioners.size())) | color(Color::GreenLight) | bold
        })
    }) | border | size(WIDTH, EQUAL, 70);
}

Element stats_box() {
    return vbox({
        text("📊 STATISTICS") | color(Color::Yellow) | bold | center,
        separator(),
        hbox({
            text("Total Requests: ") | color(Color::White),
            text(std::to_string(total_requests.load())) | color(Color::GreenLight) | bold
        }),
        hbox({
            text("Pionniers Served: ") | color(Color::White),
            text(std::to_string(Pioners.size())) | color(Color::Magenta) | bold
        })
    }) | border | size(WIDTH, EQUAL, 40);
}

Element status_box() {
    Color status_color = server_running.load() ? Color::GreenLight : Color::Red;
    std::string status_text = server_running.load() ? "● ONLINE" : "● OFFLINE";

    return vbox({
        text("🔧 STATUS") | color(Color::Yellow) | bold | center,
        separator(),
        hbox({
            text("Server: ") | color(Color::White),
            text(status_text) | color(status_color) | bold
        }),
        hbox({
            text("Port: ") | color(Color::White),
            text("5002") | color(Color::Magenta) | bold
        }),
        text(""),
        text("🌐 Onion Address:") | color(Color::Yellow) | bold,
        text(onion_address.empty() ? "Initializing..." : onion_address) | color(Color::GreenLight) | dim,
        text(""),
        text("Endpoints:") | color(Color::White) | bold,
        text("  GET  /get_pionniers") | color(Color::Cyan),
        text("  POST /add_pionnier") | color(Color::Magenta)
    }) | border | flex;
}

Element logs_box() {
    Elements log_elements;
    {
        std::lock_guard<std::mutex> lk(logs_mtx);
        if (logs.empty()) {
            log_elements.push_back(text("No logs yet...") | color(Color::GrayDark) | center);
        } else {
            for (auto it = logs.rbegin(); it != logs.rend(); ++it) {
                Color c = it->type == 1 ? Color::GreenLight :
                         (it->type == 2 ? Color::Red : Color::White);
                log_elements.push_back(
                    hbox({
                        text("[" + it->timestamp + "] ") | color(Color::GrayLight),
                        text(it->message) | color(c)
                    })
                );
            }
        }
    }

    return vbox({
        text("📝 LOGS") | color(Color::Yellow) | bold | center,
        separator(),
        vbox(log_elements) | vscroll_indicator | frame | flex
    }) | border | flex;
}

// ---------------------- Main -------------------------
int main() {
    SimpleTorLauncher* tor_launcher = nullptr;

    try {
        fs::path exe_folder = fs::current_path();
        auto screen = ScreenInteractive::Fullscreen();


        std::thread tor_thread([&]() {
            try {
                tor_launcher = new SimpleTorLauncher(exe_folder, "gate", 9052, 5002);
                onion_address = tor_launcher->launch();
                tor_ready = true;

             
                while (server_running.load()) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            } catch (const std::exception& e) {
                add_log(std::string("Tor error: ") + e.what(), 2);
            }
        });

        std::thread server_thread([&]() {
            try {
                while (!tor_ready.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                add_log("Starting HTTP server on 127.0.0.1:5002", 0);
                net::io_context ioc{1};
                tcp::acceptor acceptor{ioc, {tcp::v4(), 5002}};
                server_running = true;
                add_log("Gate ready to serve pionniers", 1);
                screen.PostEvent(Event::Custom);

                while (server_running.load()) {
                    tcp::socket socket{ioc};
                    acceptor.accept(socket);

                    beast::flat_buffer buffer;
                    http::request<http::string_body> req;
                    http::read(socket, buffer, req);

                    http::response<http::string_body> res;
                    res.version(req.version());
                    res.keep_alive(false);

                    handle_request(req, res);
                    http::write(socket, res);

                    beast::error_code ec;
                    socket.shutdown(tcp::socket::shutdown_send, ec);

                    screen.PostEvent(Event::Custom);
                }
            } catch (const std::exception& e) {
                add_log(std::string("Server error: ") + e.what(), 2);
            }
        });

        auto component = Renderer([&] {
            return vbox({
                gate_banner(),
                separator(),
                hbox({
                    vbox({
                        status_box(),
                        text("") | size(HEIGHT, EQUAL, 1),
                        stats_box()
                    }) | size(WIDTH, EQUAL, 42),
                    separator(),
                    vbox({
                        pioneers_box(),
                        text("") | size(HEIGHT, EQUAL, 1),
                        logs_box()
                    }) | flex
                }) | flex,
                separator(),
                hbox({
                    text("🔑 Press ") | color(Color::White),
                    text("Q") | color(Color::Red) | bold,
                    text(" to quit") | color(Color::White),
                    filler(),
                    text(server_running.load() ? "⚡ RUNNING" : "⏸ STOPPED") |
                        color(server_running.load() ? Color::GreenLight : Color::Red) | bold
                })
            }) | border;
        });

        component |= CatchEvent([&](Event event) {
            if (event == Event::Character('q') || event == Event::Character('Q')) {
                server_running = false;
                screen.ExitLoopClosure()();
                return true;
            }
            return false;
        });

    
        std::thread refresh_thread([&]() {
            while (server_running.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                screen.PostEvent(Event::Custom);
            }
        });

        screen.Loop(component);
        server_running = false;
        add_log("Shutting down...", 0);

        if (tor_launcher) {
            tor_launcher->stop();
            delete tor_launcher;
        }

        if (tor_thread.joinable()) tor_thread.join();
        if (server_thread.joinable()) server_thread.join();
        if (refresh_thread.joinable()) refresh_thread.join();

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        if (tor_launcher) {
            tor_launcher->stop();
            delete tor_launcher;
        }
        return 1;
    }

    return 0;
}
