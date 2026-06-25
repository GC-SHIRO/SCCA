#pragma once

#include <string>
#include <vector>

#include "types.hpp"

class TUI {
public:
    TUI();

    void render(const std::vector<Message>& history, int totalTokens, const std::string& status = "");
    void showHelp();
    void showError(const std::string& message);
    void showInfo(const std::string& message);
    std::string readInput();

private:
    int width_ = 100;
    int height_ = 30;

    void updateSize();
    void enableAnsiOnWindows();
    std::vector<std::string> wrap(const std::string& text, std::size_t maxWidth) const;
    std::string colorForRole(const std::string& role) const;
};
