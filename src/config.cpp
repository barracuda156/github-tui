#include "config.h"
#include <toml++/toml.h>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <cstdlib>

Config& Config::instance() {
    static Config instance;
    return instance;
}

std::string Config::get_config_dir() const {
    const char* home = getenv("HOME");
    if (!home) {
        home = getpwuid(getuid())->pw_dir;
    }
    return std::string(home) + "/.config/github-tui";
}

std::string Config::get_config_path() const {
    return get_config_dir() + "/config.toml";
}

void Config::ensure_config_dir() const {
    std::string dir = get_config_dir();
    mkdir(dir.c_str(), 0700);
}

bool Config::load() {
    try {
        auto config = toml::parse_file(get_config_path());
        auto token = config["github"]["token"].value<std::string>();
        if (token) {
            token_ = *token;
            // Strip any leading/trailing whitespace or quotes that might have been inadvertently included
            size_t start = token_.find_first_not_of(" \t\n\r'\"");
            size_t end = token_.find_last_not_of(" \t\n\r'\"");
            if (start != std::string::npos && end != std::string::npos) {
                token_ = token_.substr(start, end - start + 1);
            }
        }

        // Load optional starting_page from ui section
        auto starting_page = config["ui"]["starting_page"].value<std::string>();
        if (starting_page) {
            starting_page_ = *starting_page;
        }

        return !token_.empty();  // Return true if we at least got a token
    } catch (...) {
        return false;
    }
}

bool Config::save() {
    ensure_config_dir();

    toml::table config;
    config.insert_or_assign("github", toml::table{
        {"token", token_}
    });

    std::ofstream file(get_config_path());
    if (!file) {
        return false;
    }

    file << config;
    chmod(get_config_path().c_str(), 0600);
    return true;
}

std::optional<std::string> Config::get_token() const {
    if (token_.empty()) {
        return std::nullopt;
    }
    return token_;
}

void Config::set_token(const std::string& token) {
    token_ = token;
}

std::optional<std::string> Config::get_starting_page() const {
    if (starting_page_.empty()) {
        return std::nullopt;
    }
    return starting_page_;
}

void Config::set_starting_page(const std::string& starting_page) {
    starting_page_ = starting_page;
}
