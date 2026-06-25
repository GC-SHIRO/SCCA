#include "tui.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>

#include "utf8_utils.hpp"

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

namespace {
constexpr const char* RESET = "\033[0m";
constexpr const char* BOLD = "\033[1m";
constexpr const char* DIM = "\033[2m";
constexpr const char* CYAN = "\033[36m";
constexpr const char* GREEN = "\033[32m";
constexpr const char* YELLOW = "\033[33m";
constexpr const char* RED = "\033[31m";
constexpr const char* BLUE = "\033[34m";
constexpr const char* MAGENTA = "\033[35m";

struct RenderLine {
    std::string role;
    std::string text;
};

std::string padRight(std::string text, std::size_t width) {
    if (text.size() < width) text += std::string(width - text.size(), ' ');
    if (text.size() > width) text = text.substr(0, width);
    return text;
}

std::size_t utf8CharSize(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c >> 5) == 0x6) return 2;
    if ((c >> 4) == 0xE) return 3;
    if ((c >> 3) == 0x1E) return 4;
    return 1;
}
}

TUI::TUI() {
    enableAnsiOnWindows();
    updateSize();
}

void TUI::enableAnsiOnWindows() {
#ifdef _WIN32
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out == INVALID_HANDLE_VALUE) return;
    DWORD mode = 0;
    if (!GetConsoleMode(out, &mode)) return;
    SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleOutputCP(CP_UTF8);
#endif
}

void TUI::updateSize() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        width_ = std::max(80, info.srWindow.Right - info.srWindow.Left + 1);
        height_ = std::max(24, info.srWindow.Bottom - info.srWindow.Top + 1);
    }
#else
    width_ = 100;
    height_ = 30;
#endif
}

std::string TUI::colorForRole(const std::string& role) const {
    if (role == "user") return std::string(CYAN);
    if (role == "assistant") return std::string(GREEN);
    if (role == "tool") return std::string(YELLOW);
    if (role == "system") return std::string(MAGENTA);
    return std::string(RESET);
}

std::vector<std::string> TUI::wrap(const std::string& text, std::size_t maxWidth) const {
    std::vector<std::string> lines;
    std::istringstream input(text);
    std::string raw;
    while (std::getline(input, raw)) {
        if (raw.empty()) {
            lines.push_back("");
            continue;
        }
        std::string current;
        std::size_t visual = 0;
        for (std::size_t pos = 0; pos < raw.size();) {
            std::size_t len = utf8CharSize(static_cast<unsigned char>(raw[pos]));
            if (pos + len > raw.size()) len = 1;
            if (visual >= maxWidth && !current.empty()) {
                lines.push_back(current);
                current.clear();
                visual = 0;
            }
            current += raw.substr(pos, len);
            visual += (len == 1 ? 1 : 2);
            pos += len;
        }
        if (!current.empty()) {
            lines.push_back(current);
        }
    }
    if (lines.empty()) lines.push_back("");
    return lines;
}

void TUI::render(const std::vector<Message>& history, int totalTokens, const std::string& status) {
    updateSize();
    const int contentHeight = std::max(8, height_ - 6);
    const int boxWidth = std::max(50, width_ - 3);
    const int innerWidth = std::max(40, boxWidth - 4);
    std::vector<RenderLine> rendered;

    for (const auto& msg : history) {
        std::string label = msg.role == "user" ? " USER " : msg.role == "assistant" ? " AGENT " : " TOOL ";
        std::string text = msg.display_content.empty() ? msg.content : msg.display_content;
        if (!msg.tool_calls.empty()) {
            for (const auto& call : msg.tool_calls) {
                text += "\nTool requested: " + call.name;
            }
        }
        auto lines = wrap(text, static_cast<std::size_t>(innerWidth));
        std::string top = "+ " + label + std::string(std::max(1, boxWidth - static_cast<int>(label.size()) - 2), '-');
        rendered.push_back({msg.role, top});
        for (const auto& line : lines) {
            rendered.push_back({msg.role, "| " + line});
        }
        rendered.push_back({msg.role, "+" + std::string(boxWidth - 1, '-')});
        rendered.push_back({"", ""});
    }

    int start = std::max(0, static_cast<int>(rendered.size()) - contentHeight);
    std::cout << "\033[2J\033[H";
    std::string title = " SCCA v1.0 ";
    std::string token = " Context: " + std::to_string(totalTokens) + "/200K Tokens ";
    std::cout << BOLD << "+" << title
              << std::string(std::max(1, width_ - static_cast<int>(title.size()) - static_cast<int>(token.size()) - 2), '-')
              << token << RESET << "\n";

    for (int i = start; i < static_cast<int>(rendered.size()); ++i) {
        std::cout << colorForRole(rendered[i].role) << rendered[i].text << RESET << "\n";
    }
    for (int i = static_cast<int>(rendered.size()) - start; i < contentHeight; ++i) {
        std::cout << "\n";
    }

    std::cout << BOLD << "+" << std::string(std::max(1, width_ - 2), '-') << RESET << "\n";
    std::cout << DIM << "[Enter send] [/help] [/settings] [/clear] [/exit]" << RESET;
    if (!status.empty()) std::cout << "  " << status;
    std::cout << "\n> " << std::flush;
}

void TUI::showHelp() {
    std::cout << BLUE
              << "\nCommands:\n"
              << "  /help or /?          Show help\n"
              << "  /clear              Clear conversation history\n"
              << "  /tools              List available tools\n"
              << "  /settings           Edit API key, base URL, model, max tokens\n"
              << "  /skill add <name>   Create a simple shell skill template\n"
              << "  /exit               Quit\n\n"
              << RESET;
}

void TUI::showError(const std::string& message) {
    std::cout << RED << "\nError: " << message << RESET << "\n";
}

void TUI::showInfo(const std::string& message) {
    std::cout << GREEN << "\n" << message << RESET << "\n";
}

std::string TUI::readInput() {
    std::string input;
    std::getline(std::cin, input);
    return utf8::ensure(input);
}
