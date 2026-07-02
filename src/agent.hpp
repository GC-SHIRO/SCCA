#pragma once

#include <string>
#include <vector>

#include "config.hpp"
#include "llm_client.hpp"
#include "skill.hpp"
#include "tools.hpp"
#include "tui.hpp"

class SCCA {
public:
    explicit SCCA(std::string workspace); // 禁止隐式类型转换
    void run();

private:
    std::string workspace_;
    TUI tui_;
    ToolRegistry tools_;
    SkillManager skills_;
    LLMClient llm_;
    ConfigStore config_;
    std::vector<Message> history_;
    int totalTokens_ = 0;

    void processInput(const std::string& input);
    void agentLoop(const std::string& userMsg);
    void handleCommand(const std::string& input);
    void addAssistant(const std::string& text, std::vector<ToolCall> calls = {});
    void addToolResult(const ToolCall& call, const json& result);
    void showTools();
    void showSettings();
};
