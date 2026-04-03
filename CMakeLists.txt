cmake_minimum_required(VERSION 3.15)
project(github-tui VERSION 0.0.2 LANGUAGES CXX)

# On legacy macOS with libstdc++ runtime you will need to use a wrapper script that sets
# DYLD_LIBRARY_PATH to libgcc installation. See redirect_bins option of legacysupport.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# FTXUI
find_package(ftxui QUIET)
if(NOT ftxui_FOUND)
    message(STATUS "FTXUI not found, fetching from source")
    FetchContent_Declare(
        ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
        GIT_TAG v6.1.9
    )
    FetchContent_MakeAvailable(ftxui)
else()
    message(STATUS "Using system FTXUI")
endif()

# nlohmann/json
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann/json not found, fetching from source")
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(json)
else()
    message(STATUS "Using system nlohmann/json")
endif()

# toml++
find_package(tomlplusplus QUIET)
if(NOT tomlplusplus_FOUND)
    message(STATUS "toml++ not found, fetching from source")
    FetchContent_Declare(
        tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG v3.4.0
    )
    FetchContent_MakeAvailable(tomlplusplus)
else()
    message(STATUS "Using system toml++")
endif()

# cxxopts
find_package(cxxopts QUIET)
if(NOT cxxopts_FOUND)
    message(STATUS "cxxopts not found, fetching from source")
    FetchContent_Declare(
        cxxopts
        GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
        GIT_TAG v3.1.1
    )
    FetchContent_MakeAvailable(cxxopts)
else()
    message(STATUS "Using system cxxopts")
endif()

# cpr (requires libcurl)
find_package(cpr QUIET)
if(NOT cpr_FOUND)
    message(STATUS "cpr not found, fetching from source")
    find_package(CURL REQUIRED)
    FetchContent_Declare(
        cpr
        GIT_REPOSITORY https://github.com/libcpr/cpr.git
        GIT_TAG 1.10.5
    )
    FetchContent_MakeAvailable(cpr)
else()
    message(STATUS "Using system cpr")
endif()

add_executable(github-tui
    src/main.cpp
    src/config.cpp
    src/github_client.cpp
    src/ui/app.cpp
)

target_include_directories(github-tui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(github-tui PRIVATE
    ftxui::screen
    ftxui::dom
    ftxui::component
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
    cxxopts::cxxopts
    cpr::cpr
)

if(APPLE)
    target_compile_definitions(github-tui PRIVATE MACOS)
endif()

# Install targets
install(TARGETS github-tui
    RUNTIME DESTINATION bin
)
