# SCCA - 简易 TUI Claude Code Assistant

SCCA 是一个使用 C++17 编写的终端 AI 编程助手演示项目。它通过 Anthropic Messages API 与模型对话，在终端中展示 Agent 的回复、工具调用过程，并提供安全的文件操作、命令执行、Python 代码执行和简化 Skill 加载能力。

## 功能概览

- 终端 TUI 对话界面
- 多轮对话历史与 Token 统计
- Anthropic API 调用，支持自定义 `base_url` 和 `model`
- 内置文件工具、命令工具、Python 执行工具
- 文件操作限制在当前工作目录内
- 启动时自动加载 `skills/` 目录下的 JSON Skill

## 构建

推荐使用 CMake：

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

如果 CMake 在 Windows 中文路径下异常退出，可以直接使用 MinGW g++ 编译：

```powershell
g++ -std=c++17 -DNOMINMAX -Isrc -Ithird_party src/main.cpp src/tui.cpp src/tools.cpp src/path_utils.cpp src/utf8_utils.cpp src/config.cpp src/llm_client.cpp src/skill.cpp src/agent.cpp -o scca.exe
```

## 运行

先设置 API Key：

```powershell
$env:ANTHROPIC_API_KEY="your_api_key_here"
.\scca.exe
```

如果使用 CMake 产物：

```powershell
.\build\scca.exe
```

Linux/macOS：

```bash
export ANTHROPIC_API_KEY="your_api_key_here"
./build/scca
```

## 配置模型和 Base URL

程序支持两种配置方式：

1. 在终端中设置环境变量。
2. 在程序内输入 `/settings`，进入设置页后保存到 `.scca/config.json`，下次启动自动读取。

环境变量示例：

```powershell
$env:ANTHROPIC_API_KEY="your_api_key_here"
$env:SCCA_BASE_URL="https://api.anthropic.com"
$env:SCCA_MODEL="claude-3-haiku-20240307"
.\scca.exe
```

`SCCA_BASE_URL` 可以填写根地址，也可以填写完整 Messages API 地址：

```powershell
$env:SCCA_BASE_URL="https://api.anthropic.com"
$env:SCCA_BASE_URL="https://api.anthropic.com/v1/messages"
```

默认值：

- `SCCA_BASE_URL`: `https://api.anthropic.com`
- `SCCA_MODEL`: `claude-3-haiku-20240307`

注意：当前请求体使用 Anthropic Messages API 格式，因此自定义 `base_url` 需要兼容 Anthropic 的 `/v1/messages` 接口。

`/settings` 会持久化保存 API Key，便于课程演示。正式项目中不建议把密钥明文保存在项目目录。

## 内置命令

- `/help` 或 `/?`：查看帮助
- `/tools`：查看已注册工具
- `/settings` 或 `/setting`：进入设置页，修改 API Key、Base URL、模型和最大 Token
- `/clear`：清空当前对话
- `/skill add <name>`：在 `skills/` 下创建 Skill 模板
- `/exit`：退出程序

## 内置工具

- `read_file(path)`：读取文件
- `write_file(path, content)`：创建或覆盖文件
- `append_file(path, content)`：追加文件内容
- `delete_file(path)`：删除文件
- `list_dir(path)`：列出目录内容
- `execute_command(command, timeout?)`：执行命令
- `run_python(code)`：执行 Python 代码片段

所有文件工具都限制在当前工作目录内。命令工具会拦截若干明显危险的命令模式，便于课程演示时降低误操作风险。

## 项目结构

```text
src/main.cpp          程序入口
src/tui.*             ANSI 终端界面渲染
src/agent.*           对话历史和 Agent 工具循环
src/tools.*           工具注册表和内置工具
src/llm_client.*      通过 curl 调用 Anthropic API
src/config.*          本地持久化配置
src/skill.*           JSON Skill 加载器
src/path_utils.*      跨平台路径工具
src/utf8_utils.*      Windows 中文输入和工具输出的 UTF-8 转换
skills/example.json   示例 Skill
third_party/json.hpp  nlohmann/json 单头文件
```
