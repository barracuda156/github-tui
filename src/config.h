#pragma once

#include <string>
#include <optional>

class Config {
public:
    static Config& instance();

    bool load();
    bool save();

    std::optional<std::string> get_token() const;
    void set_token(const std::string& token);

    std::optional<std::string> get_starting_page() const;
    void set_starting_page(const std::string& starting_page);

    std::string get_config_path() const;

private:
    Config() = default;
    std::string token_;
    std::string starting_page_;

    std::string get_config_dir() const;
    void ensure_config_dir() const;
};
