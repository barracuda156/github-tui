#include "config.h"
#include "github_client.h"
#include "ui/app.h"
#include <cxxopts.hpp>
#include <iostream>
#include <memory>

int main(int argc, char* argv[]) {
    cxxopts::Options options("github-tui", "Terminal UI for GitHub");

    options.add_options()
        ("t,token", "GitHub Personal Access Token", cxxopts::value<std::string>())
        ("h,help", "Print usage");

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    auto& config = Config::instance();

    if (result.count("token")) {
        config.set_token(result["token"].as<std::string>());
        if (config.save()) {
            std::cout << "Token saved to " << config.get_config_path() << std::endl;
        } else {
            std::cerr << "Failed to save token" << std::endl;
            return 1;
        }
        return 0;
    }

    config.load();
    auto token = config.get_token();

    std::string token_str;
    if (token) {
        token_str = *token;
    } else {
        std::cout << "Running without authentication (read-only, public repos only).\n"
                  << "Rate limit: 60 requests/hour.\n\n"
                  << "To access private repos and increase rate limit to 5000/hour:\n"
                  << "  " << argv[0] << " --token YOUR_GITHUB_TOKEN\n"
                  << "  Generate token at: https://github.com/settings/tokens\n\n";
    }

    auto client = std::make_shared<GitHubClient>(token_str);
    App app(client);
    app.run();

    return 0;
}
