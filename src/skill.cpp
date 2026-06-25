#include "skill.hpp"

#include <fstream>
#include <sstream>

#include "path_utils.hpp"

namespace {
std::string replaceAll(std::string text, const std::string& needle, const std::string& value) {
    std::size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        text.replace(pos, needle.size(), value);
        pos += value.size();
    }
    return text;
}
}

SkillManager::SkillManager(std::string skillDir)
    : skillDir_(std::move(skillDir)) {}

int SkillManager::loadInto(ToolRegistry& registry) const {
    if (!pathutil::exists(skillDir_)) pathutil::createDirectories(skillDir_);
    int loaded = 0;
    for (const auto& entry : pathutil::listDirectory(skillDir_)) {
        if (entry.is_dir || entry.name.size() < 6 || entry.name.substr(entry.name.size() - 5) != ".json") continue;
        try {
            std::string file = pathutil::join(skillDir_, entry.name);
            std::ifstream in(file, std::ios::binary);
            json spec = json::parse(in);
            std::string name = spec.at("name").get<std::string>();
            std::string description = spec.value("description", "User skill");
            json inputSchema = spec.value("input_schema", json{{"type", "object"}});
            json impl = spec.value("impl", json::object());

            registry.addTool({
                name,
                description,
                inputSchema,
                [impl, name, &registry](const json& params) -> json {
                    if (impl.value("type", "") != "shell") {
                        return {{"ok", false}, {"error", "skill '" + name + "' has no executable impl"}};
                    }
                    std::string command = impl.value("command", "");
                    for (auto it = params.begin(); it != params.end(); ++it) {
                        std::string value = it.value().is_string() ? it.value().get<std::string>() : it.value().dump();
                        command = replaceAll(command, "{{" + it.key() + "}}", value);
                    }
                    ToolCall call{"skill_" + name, "execute_command", {{"command", command}}};
                    return registry.execute(call);
                }
            });
            ++loaded;
        } catch (...) {
            // Invalid skills are skipped so one bad file does not break startup.
        }
    }
    return loaded;
}

bool SkillManager::createShellSkillTemplate(const std::string& name) const {
    if (name.empty()) return false;
    pathutil::createDirectories(skillDir_);
    std::string file = pathutil::join(skillDir_, name + ".json");
    if (pathutil::exists(file)) return false;
    json spec = {
        {"name", name},
        {"description", "Describe what this skill does."},
        {"input_schema", {
            {"type", "object"},
            {"properties", {
                {"text", {{"type", "string"}}}
            }},
            {"required", json::array({"text"})}
        }},
        {"impl", {
            {"type", "shell"},
#ifdef _WIN32
            {"command", "echo {{text}}"}
#else
            {"command", "printf '%s\\n' '{{text}}'"}
#endif
        }}
    };
    std::ofstream out(file, std::ios::binary);
    out << spec.dump(2);
    return true;
}
