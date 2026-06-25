#pragma once

#include <string>
#include <vector>

namespace pathutil {

struct DirItem {
    std::string name;
    bool is_dir = false;
    long long size = 0;
};

std::string currentPath();
std::string tempPath();
std::string join(const std::string& left, const std::string& right);
std::string parentPath(const std::string& path);
std::string filename(const std::string& path);
std::string absolutePath(const std::string& path);
std::string normalizePath(const std::string& path);
bool isAbsolute(const std::string& path);
bool exists(const std::string& path);
bool isDirectory(const std::string& path);
bool createDirectories(const std::string& path);
bool removeFile(const std::string& path);
std::vector<DirItem> listDirectory(const std::string& path);
std::string lowerForCompare(std::string text);

}
