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

#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <algorithm>

#include "client/network/network.hpp"
#include "client/config.hpp"
#include "client/pionniers/pionniers.hpp"

size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

std::pair<int, std::string> fetch_url_with_status(const std::string &url) {
    CURL *curl = curl_easy_init();
    if (!curl) return std::make_pair(0, std::string());

    std::string response;
    long http_code = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_PROXY, "socks5h://127.0.0.1:9050");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    }

    curl_easy_cleanup(curl);
    return std::make_pair(static_cast<int>(http_code), response);
}

std::vector<std::string> fetch_servers_from_gates() {
    std::vector<std::string> servers;

    for (const auto &gate : gates) {
        std::string url = "http://" + gate + "/get_pionniers";
        std::pair<int, std::string> result = fetch_url_with_status(url);
        int status = result.first;
        std::string resp = result.second;

        if (status != 200 || resp.empty()) continue;

        std::stringstream ss(std::move(resp));
        std::string line;
        while (std::getline(ss, line)) {
            // trim
            line.erase(line.begin(), std::find_if(line.begin(), line.end(),
                [](unsigned char ch){ return !std::isspace(ch); }));
            line.erase(std::find_if(line.rbegin(), line.rend(),
                [](unsigned char ch){ return !std::isspace(ch); }).base(), line.end());

            if (!line.empty() && line.find(".onion") != std::string::npos) {
                if (std::find(servers.begin(), servers.end(), line) == servers.end()) {
                    servers.push_back(line);
                }
            }
        }
    }

    return servers;
}

bool fetch_posts() {
    posts_cache.clear();

    std::vector<std::string> servers;
    {
        std::lock_guard<std::recursive_mutex> lg(pioneers_mutex);
        servers = pioneers;
    }

    if (servers.empty()) {
        std::cerr << "[ERROR] No pioneers available for fetching posts\n";
        return false;
    }

    std::cerr << "[INFO] Fetching from " << servers.size() << " pioneer(s)\n";

    bool any_success = false;
    const std::string delimiter = "\n---END---\n";

    for (const auto &server : servers) {
        std::string url = "http://" + server + "/get_posts";
        std::cerr << "[INFO] Fetching posts from: " << url << "\n";

        auto [status, resp] = fetch_url_with_status(url);

        if (status != 200) {
            std::cerr << "[WARN] " << url << " returned HTTP " << status << "\n";
            continue;
        }

        if (resp.empty()) {
            std::cerr << "[INFO] " << url << " has no posts yet\n";
            continue;
        }

        size_t pos = 0;
        while ((pos = resp.find(delimiter)) != std::string::npos) {
            std::string post = resp.substr(0, pos);

            post.erase(post.begin(), std::find_if(post.begin(), post.end(),
                [](unsigned char ch){ return !std::isspace(ch); }));
            post.erase(std::find_if(post.rbegin(), post.rend(),
                [](unsigned char ch){ return !std::isspace(ch); }).base(), post.end());

            if (!post.empty()) {
                posts_cache.push_back(post);
            }

            resp.erase(0, pos + delimiter.size());
        }

        resp.erase(resp.begin(), std::find_if(resp.begin(), resp.end(),
            [](unsigned char ch){ return !std::isspace(ch); }));
        resp.erase(std::find_if(resp.rbegin(), resp.rend(),
            [](unsigned char ch){ return !std::isspace(ch); }).base(), resp.end());
        if (!resp.empty()) posts_cache.push_back(resp);

        any_success = true;
    }

    std::cerr << "[INFO] Total posts fetched: " << posts_cache.size() << "\n";
    return any_success;
}

bool send_post_to_all(const std::string &post) {
    std::vector<std::string> servers;
    {
        std::lock_guard<std::recursive_mutex> lg(pioneers_mutex);
        servers = pioneers;
    }

    if (servers.empty()) {
        std::cerr << "[ERROR] No pioneers available for posting\n";
        return false;
    }

    bool any_ok = false;

    for (const auto &server : servers) {
        CURL *curl = curl_easy_init();
        if (!curl) continue;

        std::string url = "http://" + server + "/add_post";
        std::cerr << "[INFO] Posting to: " << url << "\n";

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post.c_str());
        curl_easy_setopt(curl, CURLOPT_PROXY, "socks5h://127.0.0.1:9050");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code == 201 || http_code == 200) {
                std::cerr << "[OK] Post published to " << server << "\n";
                any_ok = true;
            } else {
                std::cerr << "[WARN] " << server << " returned HTTP " << http_code << "\n";
            }
        } else {
            std::cerr << "[ERROR] Failed to connect to " << server << ": "
                      << curl_easy_strerror(res) << "\n";
        }

        curl_easy_cleanup(curl);
    }

    return any_ok;
}
