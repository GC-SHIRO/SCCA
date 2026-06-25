#include "path_utils.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#endif

namespace pathutil {

namespace {
char sep() {
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

bool mkdirOne(const std::string& path) {
    if (path.empty() || exists(path)) return true;
#ifdef _WIN32
    return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}
}

std::string currentPath() {
    char buffer[4096]{};
#ifdef _WIN32
    DWORD len = GetCurrentDirectoryA(sizeof(buffer), buffer);
    if (len == 0 || len >= sizeof(buffer)) throw std::runtime_error("cannot get current directory");
#else
    if (!getcwd(buffer, sizeof(buffer))) throw std::runtime_error("cannot get current directory");
#endif
    return normalizePath(buffer);
}

std::string tempPath() {
#ifdef _WIN32
    char buffer[MAX_PATH + 1]{};
    DWORD len = GetTempPathA(MAX_PATH, buffer);
    if (len == 0 || len > MAX_PATH) return ".";
    return normalizePath(buffer);
#else
    const char* tmp = std::getenv("TMPDIR");
    return normalizePath(tmp ? tmp : "/tmp");
#endif
}

bool isAbsolute(const std::string& path) {
    if (path.empty()) return false;
#ifdef _WIN32
    return path.size() > 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':';
#else
    return path[0] == '/';
#endif
}

std::string join(const std::string& left, const std::string& right) {
    if (left.empty() || isAbsolute(right)) return normalizePath(right);
    if (right.empty()) return normalizePath(left);
    char s = sep();
    if (left.back() == '/' || left.back() == '\\') return normalizePath(left + right);
    return normalizePath(left + s + right);
}

std::string normalizePath(const std::string& input) {
    std::string path = input;
    std::replace(path.begin(), path.end(), '/', sep());
    std::vector<std::string> parts;
    std::string root;
#ifdef _WIN32
    if (path.size() >= 2 && path[1] == ':') {
        root = path.substr(0, 2);
        path = path.substr(2);
        if (!path.empty() && (path[0] == '\\' || path[0] == '/')) root += sep();
    } else if (!path.empty() && (path[0] == '\\' || path[0] == '/')) {
        root = std::string(1, sep());
    }
#else
    if (!path.empty() && path[0] == '/') root = "/";
#endif
    std::string token;
    for (char c : path) {
        if (c == '/' || c == '\\') {
            if (!token.empty() && token != ".") {
                if (token == ".." && !parts.empty() && parts.back() != "..") parts.pop_back();
                else if (token != ".." || root.empty()) parts.push_back(token);
            }
            token.clear();
        } else {
            token += c;
        }
    }
    if (!token.empty() && token != ".") {
        if (token == ".." && !parts.empty() && parts.back() != "..") parts.pop_back();
        else if (token != ".." || root.empty()) parts.push_back(token);
    }

    std::string out = root;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (!out.empty() && out.back() != sep()) out += sep();
        out += parts[i];
    }
    if (out.empty()) out = ".";
    return out;
}

std::string absolutePath(const std::string& path) {
    if (isAbsolute(path)) return normalizePath(path);
    return join(currentPath(), path);
}

std::string parentPath(const std::string& path) {
    std::string p = normalizePath(path);
    auto pos = p.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    if (pos == 0) return p.substr(0, 1);
    if (pos == 2 && p.size() > 2 && p[1] == ':') return p.substr(0, 3);
    return p.substr(0, pos);
}

std::string filename(const std::string& path) {
    std::string p = normalizePath(path);
    auto pos = p.find_last_of("/\\");
    return pos == std::string::npos ? p : p.substr(pos + 1);
}

bool exists(const std::string& path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0;
}

bool isDirectory(const std::string& path) {
    struct stat st {};
    if (stat(path.c_str(), &st) != 0) return false;
    return (st.st_mode & S_IFDIR) != 0;
}

bool createDirectories(const std::string& path) {
    if (path.empty() || exists(path)) return true;
    std::string normalized = normalizePath(path);
    std::string partial;
#ifdef _WIN32
    if (normalized.size() >= 2 && normalized[1] == ':') {
        partial = normalized.substr(0, 2);
        normalized = normalized.substr(2);
    }
#endif
    std::string token;
    for (char c : normalized) {
        if (c == '/' || c == '\\') {
            if (!token.empty()) {
                partial = partial.empty() ? token : join(partial, token);
                if (!mkdirOne(partial)) return false;
                token.clear();
            }
        } else {
            token += c;
        }
    }
    if (!token.empty()) {
        partial = partial.empty() ? token : join(partial, token);
        if (!mkdirOne(partial)) return false;
    }
    return true;
}

bool removeFile(const std::string& path) {
    return std::remove(path.c_str()) == 0;
}

std::vector<DirItem> listDirectory(const std::string& path) {
    std::vector<DirItem> items;
#ifdef _WIN32
    std::string pattern = join(path, "*");
    WIN32_FIND_DATAA data{};
    HANDLE h = FindFirstFileA(pattern.c_str(), &data);
    if (h == INVALID_HANDLE_VALUE) return items;
    do {
        std::string name = data.cFileName;
        if (name == "." || name == "..") continue;
        bool dir = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        long long size = (static_cast<long long>(data.nFileSizeHigh) << 32) + data.nFileSizeLow;
        items.push_back({name, dir, dir ? 0 : size});
    } while (FindNextFileA(h, &data));
    FindClose(h);
#else
    DIR* dir = opendir(path.c_str());
    if (!dir) return items;
    while (auto* ent = readdir(dir)) {
        std::string name = ent->d_name;
        if (name == "." || name == "..") continue;
        std::string full = join(path, name);
        struct stat st {};
        stat(full.c_str(), &st);
        bool isDir = (st.st_mode & S_IFDIR) != 0;
        items.push_back({name, isDir, isDir ? 0 : static_cast<long long>(st.st_size)});
    }
    closedir(dir);
#endif
    std::sort(items.begin(), items.end(), [](const DirItem& a, const DirItem& b) {
        return a.name < b.name;
    });
    return items;
}

std::string lowerForCompare(std::string text) {
#ifdef _WIN32
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
#endif
    return text;
}

}
