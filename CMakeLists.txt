cmake_minimum_required(VERSION 3.21)
project(TorsperClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg
set(CMAKE_TOOLCHAIN_FILE "C:/Users/roman/Documents/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

# Исполняемые файлы
add_executable(torsper_client 
    src/client/client.cpp
    src/client/network/network.cpp
    src/client/pionniers/pionniers.cpp
    src/client/ui/ui.cpp
    )
add_executable(torsper_gate src/gate/gate.cpp)
add_executable(torsper_pioner src/pionnier/pionnier.cpp)

target_include_directories(torsper_client PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(torsper_gate PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(torsper_pioner PRIVATE ${CMAKE_SOURCE_DIR}/include)


find_package(CURL REQUIRED)
find_package(ftxui REQUIRED)

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(torsper_gate PRIVATE nlohmann_json::nlohmann_json)

target_link_libraries(torsper_client PRIVATE CURL::libcurl ftxui::screen ftxui::dom ftxui::component)
target_link_libraries(torsper_gate PRIVATE CURL::libcurl ftxui::screen ftxui::dom ftxui::component)
target_link_libraries(torsper_pioner PRIVATE CURL::libcurl ftxui::screen ftxui::dom ftxui::component)

if(WIN32)
    target_link_libraries(torsper_client PRIVATE ws2_32)
    target_link_libraries(torsper_gate PRIVATE ws2_32)
    target_link_libraries(torsper_pioner PRIVATE ws2_32)
endif()
