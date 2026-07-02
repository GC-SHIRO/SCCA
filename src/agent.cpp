#include "agent.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

#include "path_utils.hpp"

SCCA::SCCA(std::string workspace)
    : workspace_(std::move(workspace)),
      tools_(workspace_),
      skills_(pathutil::join(workspace_, "skills")),
      llm_(workspace_),
      config_(workspace_) {
    tools_.registerBuiltins();
    skills_.loadInto(tools_);
}

void SCCA::run() {
    history_.push_back({"assistant", "Welcome to SCCA. Ask me to edit files, run commands, or write Python. Use /help for commands.", {}, "", ""});
    while (true) {
        tui_.render(history_, totalTokens_);
        std::string input = tui_.readInput();
        if (!std::cin.good()) break;
        if (input.empty()) continue;
        processInput(input);
    }
}

void SCCA::processInput(const std::string& input) {
    if (!input.empty() && input[0] == '/') {
        handleCommand(input);
        return;
    }
    agentLoop(input);
}

void SCCA::handleCommand(const std::string& input) {
    if (input == "/exit" || input == "/quit") {
        std::exit(0);
    }
    if (input == "/help" || input == "/?") {
        tui_.showHelp();
        std::cout << "Press Enter to continue...";
        std::string ignored;
        std::getline(std::cin, ignored);
        return;
    }
    if (input == "/clear") {
        history_.clear();
        totalTokens_ = 0;
        history_.push_back({"assistant", "Conversation cleared.", {}, ""});
        return;
    }
    if (input == "/tools") {
        showTools();
        std::cout << "Press Enter to continue...";
        std::string ignored;
        std::getline(std::cin, ignored);
        return;
    }
    if (input == "/settings" || input == "/setting") {
        showSettings();
        return;
    }
    const std::string prefix = "/skill add ";
    if (input.rfind(prefix, 0) == 0) {
        std::string name = input.substr(prefix.size());
        bool created = skills_.createShellSkillTemplate(name);
        if (created) {
            skills_.loadInto(tools_);
            history_.push_back({"assistant", "Skill template created: skills/" + name + ".json", {}, "", ""});
        } else {
            history_.push_back({"assistant", "Could not create skill. Check name or existing file.", {}, "", ""});
        }
        return;
    }
    history_.push_back({"assistant", "Unknown command. Use /help to see available commands.", {}, "", ""});
}

void SCCA::showTools() {
    std::cout << "\nAvailable tools:\n";
    for (const auto& tool : tools_.list()) {
        std::cout << "  - " << tool.name << ": " << tool.description << "\n";
    }
    std::cout << "\n";
}

void SCCA::addAssistant(const std::string& text, std::vector<ToolCall> calls) {
    history_.push_back({"assistant", text, std::move(calls), "", ""});
}

void SCCA::addToolResult(const ToolCall& call, const json& result) {
    std::ostringstream ss;
    ss << "Tool " << call.name << " result:\n" << result.dump(2);

    std::string status = result.value("ok", false) ? "success" : "failed";
    std::string output = result.value("output", "");
    if (output.empty()) output = result.value("error", "");
    long long elapsed = result.value("elapsed_ms", 0);
    std::string byteLabel = "output";
    std::size_t byteCount = output.size();
    if ((call.name == "write_file" || call.name == "append_file" || call.name == "run_python") &&
        call.arguments.contains("content")) {
        byteLabel = "written";
        byteCount = call.arguments.value("content", "").size();
    }
    if (call.name == "write_file" || call.name == "append_file") {
        byteLabel = "written";
        byteCount = call.arguments.value("content", "").size();
    }
    std::ostringstream display;
    display << "Used tool: " << call.name
            << " | " << status
            << " | " << elapsed << " ms"
            << " | " << byteLabel << " " << byteCount << " bytes";
    if (call.name == "write_file" || call.name == "append_file" || call.name == "read_file") {
        if (call.arguments.contains("path")) display << " | " << call.arguments.value("path", "");
    }
    history_.push_back({"tool", ss.str(), {}, call.id, display.str()});
}

void SCCA::agentLoop(const std::string& userMsg) {
    history_.push_back({"user", userMsg, {}, "", ""});

    for (int step = 0; step < 50; ++step) {
        tui_.render(history_, totalTokens_, "Agent is thinking...");
        try {
            LLMResponse response = llm_.complete(history_, tools_.anthropicTools());
            totalTokens_ += response.input_tokens + response.output_tokens;
            addAssistant(response.text, response.tool_calls);

            if (response.tool_calls.empty()) return;

            for (const auto& call : response.tool_calls) {
                tui_.render(history_, totalTokens_, "Running tool " + call.name + "...");
                // 计算时间
                auto started = std::chrono::steady_clock::now();
                json result = tools_.execute(call);
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - started).count();
                
                result["elapsed_ms"] = elapsed;
                addToolResult(call, result);
            }
        } catch (const std::exception& ex) {
            history_.push_back({"assistant", std::string("API or Agent error: ") + ex.what(), {}, "", ""});
            return;
        }
    }
    history_.push_back({"assistant", "Stopped after too many tool iterations. Please refine the request.", {}, "", ""});
}

void SCCA::showSettings() {
    AppConfig config = config_.load();
    while (true) {
        std::cout << "\033[2J\033[H";
        std::cout << "==================== SCCA Settings ====================\n";
        std::cout << "Config file: " << config_.configPath() << "\n\n";
        std::cout << "1. API Key  : " << (config.api_key.empty() ? "(empty)" : "********" + config.api_key.substr(config.api_key.size() > 4 ? config.api_key.size() - 4 : 0)) << "\n";
        std::cout << "2. Base URL : " << config.base_url << "\n";
        std::cout << "3. Model    : " << config.model << "\n";
        std::cout << "4. MaxToken : " << config.max_tokens << "\n";
        std::cout << "5. Clear API Key\n";
        std::cout << "q. Back\n\n";
        std::cout << "Choose item: " << std::flush;

        std::string choice = tui_.readInput();
        if (choice == "q" || choice == "Q" || choice.empty()) break;

        if (choice == "1") {
            std::cout << "New API Key: " << std::flush;
            config.api_key = tui_.readInput();
        } else if (choice == "2") {
            std::cout << "New Base URL: " << std::flush;
            std::string value = tui_.readInput();
            if (!value.empty()) config.base_url = value;
        } else if (choice == "3") {
            std::cout << "New Model: " << std::flush;
            std::string value = tui_.readInput();
            if (!value.empty()) config.model = value;
        } else if (choice == "4") {
            std::cout << "New Max Tokens: " << std::flush;
            std::string value = tui_.readInput();
            try {
                int parsed = std::stoi(value);
                if (parsed > 0) config.max_tokens = parsed;
            } catch (...) {
                std::cout << "Invalid number. Press Enter...";
                std::string ignored = tui_.readInput();
            }
        } else if (choice == "5") {
            config.api_key.clear();
        }
        config_.save(config);
    }
    history_.push_back({"assistant", "Settings saved. New API settings will be used on the next request.", {}, "", ""});
}
