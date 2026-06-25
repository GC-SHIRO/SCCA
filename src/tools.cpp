#include "tools.hpp"

#include <array>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>

#include "path_utils.hpp"
#include "utf8_utils.hpp"

namespace {
json ok(const std::string& output) {
    return {{"ok", true}, {"output", utf8::ensure(output)}};
}

json fail(const std::string& code, const std::string& message) {
    return {{"ok", false}, {"error_code", code}, {"error", utf8::ensure(message)}};
}

std::string readPipe(const std::string& command) {
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) return "failed to start command";

    std::array<char, 4096> buffer{};
    std::string result;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
        if (result.size() > 200000) {
            result += "\n[output truncated]\n";
            break;
        }
    }
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return result;
}

bool containsDangerousCommand(const std::string& command) {
    const std::vector<std::string> blocked = {
        "rm -rf /", "rm -rf /*", "dd if=/dev/zero", ":(){ :|:& };:",
        "format c:", "del /f /s /q c:\\", "rd /s /q c:\\"
    };
    std::string lower = command;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    for (const auto& item : blocked) {
        if (lower.find(item) != std::string::npos) return true;
    }
    return false;
}

json schema(std::initializer_list<std::pair<std::string, json>> props,
            std::initializer_list<std::string> required) {
    json properties = json::object();
    for (const auto& item : props) properties[item.first] = item.second;
    return {
        {"type", "object"},
        {"properties", properties},
        {"required", std::vector<std::string>(required)}
    };
}
}

ToolRegistry::ToolRegistry(std::string workspace)
    : workspace_(pathutil::absolutePath(std::move(workspace))) {}

void ToolRegistry::addTool(const ToolDefinition& tool) {
    tools_[tool.name] = tool;
}

bool ToolRegistry::has(const std::string& name) const {
    return tools_.find(name) != tools_.end();
}

std::vector<ToolDefinition> ToolRegistry::list() const {
    std::vector<ToolDefinition> result;
    for (const auto& [_, tool] : tools_) result.push_back(tool);
    return result;
}

json ToolRegistry::anthropicTools() const {
    json result = json::array();
    for (const auto& [_, tool] : tools_) {
        result.push_back({
            {"name", tool.name},
            {"description", tool.description},
            {"input_schema", tool.input_schema}
        });
    }
    return result;
}

json ToolRegistry::execute(const ToolCall& call) const {
    auto it = tools_.find(call.name);
    if (it == tools_.end()) return fail("E006", "unknown tool: " + call.name);
    try {
        return it->second.executor(call.arguments);
    } catch (const std::exception& ex) {
        return fail("E999", ex.what());
    }
}

std::string ToolRegistry::safePath(const std::string& requested) const {
    std::string combined = pathutil::isAbsolute(requested) ? requested : pathutil::join(workspace_, requested);
    std::string normalized = pathutil::normalizePath(combined);
    auto base = pathutil::lowerForCompare(workspace_);
    auto target = pathutil::lowerForCompare(normalized);
    if (target.rfind(base, 0) != 0) {
        throw std::runtime_error("E004: access denied outside workspace: " + requested);
    }
    if (normalized.find(".git") != std::string::npos) {
        throw std::runtime_error("E004: access denied to .git");
    }
    return normalized;
}

void ToolRegistry::registerBuiltins() {
    addTool({"read_file", "Read a UTF-8 text file inside the workspace.",
             schema({{"path", {{"type", "string"}}}}, {"path"}),
             [this](const json& p) { return readFile(p); }});

    addTool({"write_file", "Create or overwrite a file inside the workspace.",
             schema({{"path", {{"type", "string"}}}, {"content", {{"type", "string"}}}}, {"path", "content"}),
             [this](const json& p) { return writeFile(p); }});

    addTool({"append_file", "Append text to a file inside the workspace.",
             schema({{"path", {{"type", "string"}}}, {"content", {{"type", "string"}}}}, {"path", "content"}),
             [this](const json& p) { return appendFile(p); }});

    addTool({"delete_file", "Delete a file inside the workspace.",
             schema({{"path", {{"type", "string"}}}}, {"path"}),
             [this](const json& p) { return deleteFile(p); }});

    addTool({"list_dir", "List files and directories inside the workspace.",
             schema({{"path", {{"type", "string"}}}}, {"path"}),
             [this](const json& p) { return listDir(p); }});

    addTool({"execute_command", "Execute a shell command in the workspace and return stdout/stderr.",
             schema({{"command", {{"type", "string"}}}, {"timeout", {{"type", "integer"}}}}, {"command"}),
             [this](const json& p) { return executeCommand(p); }});

    addTool({"run_python", "Run a Python code snippet and return stdout/stderr.",
             schema({{"code", {{"type", "string"}}}}, {"code"}),
             [this](const json& p) { return runPython(p); }});
}

json ToolRegistry::readFile(const json& params) const {
    auto path = safePath(params.at("path").get<std::string>());
    std::ifstream in(path, std::ios::binary);
    if (!in) return fail("E007", "cannot open file: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ok(ss.str());
}

json ToolRegistry::writeFile(const json& params) const {
    auto path = safePath(params.at("path").get<std::string>());
    pathutil::createDirectories(pathutil::parentPath(path));
    std::ofstream out(path, std::ios::binary);
    if (!out) return fail("E008", "cannot write file: " + path);
    auto content = params.at("content").get<std::string>();
    out << content;
    return ok("file written: " + path + " (" + std::to_string(content.size()) + " bytes)");
}

json ToolRegistry::appendFile(const json& params) const {
    auto path = safePath(params.at("path").get<std::string>());
    pathutil::createDirectories(pathutil::parentPath(path));
    std::ofstream out(path, std::ios::binary | std::ios::app);
    if (!out) return fail("E009", "cannot append file: " + path);
    auto content = params.at("content").get<std::string>();
    out << content;
    return ok("file appended: " + path + " (" + std::to_string(content.size()) + " bytes)");
}

json ToolRegistry::deleteFile(const json& params) const {
    auto path = safePath(params.at("path").get<std::string>());
    if (!pathutil::exists(path)) return fail("E010", "file does not exist: " + path);
    if (pathutil::isDirectory(path)) return fail("E011", "refusing to delete directory: " + path);
    pathutil::removeFile(path);
    return ok("file deleted: " + path);
}

json ToolRegistry::listDir(const json& params) const {
    auto path = safePath(params.value("path", "."));
    if (!pathutil::isDirectory(path)) return fail("E012", "not a directory: " + path);
    json items = json::array();
    for (const auto& entry : pathutil::listDirectory(path)) {
        std::string name = utf8::ensure(entry.name);
        items.push_back({
            {"name", name},
            {"type", entry.is_dir ? "dir" : "file"},
            {"size", entry.size}
        });
    }
    return {{"ok", true}, {"items", items}, {"output", items.dump(2)}};
}

json ToolRegistry::executeCommand(const json& params) const {
    std::string command = params.at("command").get<std::string>();
    if (containsDangerousCommand(command)) {
        return fail("E013", "dangerous command blocked. Please run it manually if you really intend it.");
    }
#ifdef _WIN32
    std::string full = "cd /d \"" + workspace_ + "\" && " + command + " 2>&1";
#else
    std::string full = "cd \"" + workspace_ + "\" && " + command + " 2>&1";
#endif
    auto output = readPipe(full);
    output = utf8::ensure(output);
    if (output.empty()) output = "(no output)";
    return ok(output);
}

json ToolRegistry::runPython(const json& params) const {
    auto code = params.at("code").get<std::string>();
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::string tmp = pathutil::join(pathutil::tempPath(), "scca_code_" + std::to_string(now) + ".py");
    {
        std::ofstream out(tmp, std::ios::binary);
        out << code;
    }
#ifdef _WIN32
    std::string command = "python \"" + tmp + "\" 2>&1";
#else
    std::string command = "python3 \"" + tmp + "\" 2>&1";
#endif
    auto output = readPipe(command);
    output = utf8::ensure(output);
    pathutil::removeFile(tmp);
    if (output.empty()) output = "(no output)";
    return ok(output);
}
