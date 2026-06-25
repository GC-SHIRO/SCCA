#include "llm_client.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "config.hpp"
#include "path_utils.hpp"
#include "utf8_utils.hpp"

namespace {
const char* SYSTEM_PROMPT = R"(You are SCCA, a terminal coding assistant.
You help with programming tasks by chatting and using tools.
Rules:
1. Use tools whenever the user asks for file operations, shell commands, or Python execution.
2. Work step by step. For complex tasks, first write 3-5 short [TASK: ...] lines.
3. Never access files outside the workspace.
4. After tool use, explain what changed and what the user can run next.
5. Keep replies concise and practical.)";

std::string shellQuote(const std::string& value) {
    std::string result = "\"";
    for (char c : value) {
        if (c == '"') result += "\\\"";
        else result += c;
    }
    result += "\"";
    return result;
}

std::string messagesEndpointFromBaseUrl(std::string baseUrl) {
    while (!baseUrl.empty() && baseUrl.back() == '/') baseUrl.pop_back();
    if (baseUrl.empty()) return "https://api.anthropic.com/v1/messages";
    if (baseUrl.size() >= 12 && baseUrl.substr(baseUrl.size() - 12) == "/v1/messages") {
        return baseUrl;
    }
    return baseUrl + "/v1/messages";
}
}

LLMClient::LLMClient(std::string workspace)
    : workspace_(std::move(workspace)) {}

bool LLMClient::hasApiKey() const {
    AppConfig config = ConfigStore(workspace_.empty() ? pathutil::currentPath() : workspace_).load();
    return config.api_key.size() > 8;
}

json LLMClient::buildRequest(const std::vector<Message>& history, const json& tools,
                             const std::string& model, int maxTokens) const {
    json messages = json::array();
    for (std::size_t i = 0; i < history.size();) {
        const auto& msg = history[i];
        if (msg.role == "tool") {
            json results = json::array();
            while (i < history.size() && history[i].role == "tool") {
                results.push_back({
                    {"type", "tool_result"},
                    {"tool_use_id", history[i].tool_call_id},
                    {"content", utf8::ensure(history[i].content)}
                });
                ++i;
            }
            messages.push_back({
                {"role", "user"},
                {"content", results}
            });
            continue;
        } else if (msg.role == "assistant" && !msg.tool_calls.empty()) {
            json content = json::array();
            if (!msg.content.empty()) {
                content.push_back({{"type", "text"}, {"text", utf8::ensure(msg.content)}});
            }
            for (const auto& call : msg.tool_calls) {
                content.push_back({
                    {"type", "tool_use"},
                    {"id", call.id},
                    {"name", call.name},
                    {"input", call.arguments}
                });
            }
            messages.push_back({{"role", "assistant"}, {"content", content}});
        } else {
            messages.push_back({{"role", msg.role}, {"content", utf8::ensure(msg.content)}});
        }
        ++i;
    }

    return {
        {"model", model},
        {"max_tokens", maxTokens},
        {"system", SYSTEM_PROMPT},
        {"messages", messages},
        {"tools", tools}
    };
}

std::string LLMClient::runCurl(const std::string& requestFile, const std::string& baseUrl,
                               const std::string& apiKey) const {
    std::string responseFile = pathutil::join(pathutil::tempPath(), "scca_response_" + std::to_string(std::rand()) + ".json");
#ifdef _WIN32
    std::string curl = "curl.exe";
#else
    std::string curl = "curl";
#endif
    std::string apiHeader = "x-api-key: " + apiKey;
    std::string command = curl +
        " -sS " + shellQuote(messagesEndpointFromBaseUrl(baseUrl)) +
        " -H " + shellQuote(apiHeader) +
        " -H " + shellQuote("anthropic-version: 2023-06-01") +
        " -H " + shellQuote("content-type: application/json") +
        " --data-binary @" + shellQuote(requestFile) +
        " -o " + shellQuote(responseFile);

    int rc = std::system(command.c_str());
    if (rc != 0) {
        throw std::runtime_error("E002: curl failed with exit code " + std::to_string(rc));
    }

    std::ifstream in(responseFile, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    std::error_code ignored;
    pathutil::removeFile(responseFile);
    return ss.str();
}

LLMResponse LLMClient::complete(const std::vector<Message>& history, const json& tools) const {
    AppConfig config = ConfigStore(workspace_.empty() ? pathutil::currentPath() : workspace_).load();
    if (config.api_key.size() <= 8) {
        throw std::runtime_error("E001: please set ANTHROPIC_API_KEY first");
    }

    std::string requestFile = pathutil::join(pathutil::tempPath(), "scca_request_" + std::to_string(std::rand()) + ".json");
    {
        std::ofstream out(requestFile, std::ios::binary);
        out << buildRequest(history, tools, config.model, config.max_tokens).dump();
    }

    std::string body = runCurl(requestFile, config.base_url, config.api_key);
    pathutil::removeFile(requestFile);

    json parsed = json::parse(body);
    if (parsed.contains("error")) {
        throw std::runtime_error("E002: " + parsed["error"].dump());
    }

    LLMResponse response;
    response.input_tokens = parsed.value("usage", json::object()).value("input_tokens", 0);
    response.output_tokens = parsed.value("usage", json::object()).value("output_tokens", 0);

    for (const auto& block : parsed.value("content", json::array())) {
        std::string type = block.value("type", "");
        if (type == "text") {
            if (!response.text.empty()) response.text += "\n";
            response.text += block.value("text", "");
        } else if (type == "tool_use") {
            response.tool_calls.push_back({
                block.value("id", ""),
                block.value("name", ""),
                block.value("input", json::object())
            });
        }
    }
    return response;
}
