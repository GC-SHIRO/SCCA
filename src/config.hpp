#pragma once

#include <string>

#include "json.hpp"

using json = nlohmann::json;

struct AppConfig {
    std::string api_key;
    std::string base_url = "https://api.anthropic.com";
    std::string model = "claude-3-haiku-20240307";
    int max_tokens = 4096;
};

class ConfigStore {
public:
    explicit ConfigStore(std::string workspace);

    AppConfig load() const;
    void save(const AppConfig& config) const;
    std::string configPath() const;

private:
    std::string workspace_;
};
