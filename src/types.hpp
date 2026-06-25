#pragma once

#include <functional>
#include <string>
#include <vector>

#include "json.hpp"

using json = nlohmann::json;

struct ToolCall {
    std::string id;
    std::string name;
    json arguments;
};

struct Message {
    std::string role;
    std::string content;
    std::vector<ToolCall> tool_calls;
    std::string tool_call_id;
    std::string display_content;
};

struct ToolDefinition {
    std::string name;
    std::string description;
    json input_schema;
    std::function<json(const json&)> executor;
};

struct LLMResponse {
    std::string text;
    std::vector<ToolCall> tool_calls;
    int input_tokens = 0;
    int output_tokens = 0;
};
