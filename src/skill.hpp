#pragma once

#include <string>

#include "tools.hpp"

class SkillManager {
public:
    explicit SkillManager(std::string skillDir);

    int loadInto(ToolRegistry& registry) const;
    bool createShellSkillTemplate(const std::string& name) const;

private:
    std::string skillDir_;
};
