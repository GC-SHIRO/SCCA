cmake_minimum_required(VERSION 3.16)
project(scca LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(scca
    src/main.cpp
    src/tui.cpp
    src/tools.cpp
    src/path_utils.cpp
    src/utf8_utils.cpp
    src/config.cpp
    src/llm_client.cpp
    src/skill.cpp
    src/agent.cpp
)

target_include_directories(scca PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

if (WIN32)
    target_compile_definitions(scca PRIVATE NOMINMAX)
endif()
