#pragma once

#include <string>
#include <vector>

#include "types.hpp"

class LLMClient {
public:
    explicit LLMClient(std::string workspace = "");

    bool hasApiKey() const;
    LLMResponse complete(const std::vector<Message>& history, const json& tools) const;

private:
    std::string workspace_;

    json buildRequest(const std::vector<Message>& history, const json& tools,
                      const std::string& model, int maxTokens) const;
    std::string runCurl(const std::string& requestFile, const std::string& baseUrl,
                        const std::string& apiKey) const;
};
