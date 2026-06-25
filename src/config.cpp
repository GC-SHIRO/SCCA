#include "config.hpp"

#include <cstdlib>
#include <fstream>

#include "path_utils.hpp"
#include "utf8_utils.hpp"

namespace {
std::string envValue(const char* name) {
    const char* value = std::getenv(name);
    return value ? utf8::ensure(value) : "";
}
}

ConfigStore::ConfigStore(std::string workspace)
    : workspace_(std::move(workspace)) {}

std::string ConfigStore::configPath() const {
    return pathutil::join(pathutil::join(workspace_, ".scca"), "config.json");
}

AppConfig ConfigStore::load() const {
    AppConfig config;

    std::string envApiKey = envValue("ANTHROPIC_API_KEY");
    std::string envBaseUrl = envValue("SCCA_BASE_URL");
    std::string envModel = envValue("SCCA_MODEL");
    if (!envApiKey.empty()) config.api_key = envApiKey;
    if (!envBaseUrl.empty()) config.base_url = envBaseUrl;
    if (!envModel.empty()) config.model = envModel;

    std::ifstream in(configPath(), std::ios::binary);
    if (!in) return config;

    try {
        json data = json::parse(in);
        if (data.contains("api_key")) config.api_key = data.value("api_key", config.api_key);
        if (data.contains("base_url")) config.base_url = data.value("base_url", config.base_url);
        if (data.contains("model")) config.model = data.value("model", config.model);
        if (data.contains("max_tokens")) config.max_tokens = data.value("max_tokens", config.max_tokens);
    } catch (...) {
        return config;
    }
    return config;
}

void ConfigStore::save(const AppConfig& config) const {
    pathutil::createDirectories(pathutil::parentPath(configPath()));
    json data = {
        {"api_key", config.api_key},
        {"base_url", config.base_url},
        {"model", config.model},
        {"max_tokens", config.max_tokens}
    };
    std::ofstream out(configPath(), std::ios::binary);
    out << data.dump(2);
}
