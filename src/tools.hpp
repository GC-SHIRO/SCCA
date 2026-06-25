#pragma once

#include <map>
#include <string>
#include <vector>

#include "types.hpp"

class ToolRegistry {
public:
    explicit ToolRegistry(std::string workspace);

    void registerBuiltins();
    void addTool(const ToolDefinition& tool);
    bool has(const std::string& name) const;
    json execute(const ToolCall& call) const;
    std::vector<ToolDefinition> list() const;
    json anthropicTools() const;

    std::string workspace() const { return workspace_; }
    std::string safePath(const std::string& requested) const;

private:
    std::string workspace_;
    std::map<std::string, ToolDefinition> tools_;

    json readFile(const json& params) const;
    json writeFile(const json& params) const;
    json appendFile(const json& params) const;
    json deleteFile(const json& params) const;
    json listDir(const json& params) const;
    json executeCommand(const json& params) const;
    json runPython(const json& params) const;
};
