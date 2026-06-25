#include "utf8_utils.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace utf8 {

bool isValid(const std::string& text) {
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(text.data());
    std::size_t i = 0;
    while (i < text.size()) {
        unsigned char c = bytes[i];
        if (c <= 0x7F) {
            ++i;
        } else if ((c >> 5) == 0x6) {
            if (i + 1 >= text.size()) return false;
            if ((bytes[i + 1] >> 6) != 0x2) return false;
            i += 2;
        } else if ((c >> 4) == 0xE) {
            if (i + 2 >= text.size()) return false;
            if ((bytes[i + 1] >> 6) != 0x2 || (bytes[i + 2] >> 6) != 0x2) return false;
            i += 3;
        } else if ((c >> 3) == 0x1E) {
            if (i + 3 >= text.size()) return false;
            if ((bytes[i + 1] >> 6) != 0x2 || (bytes[i + 2] >> 6) != 0x2 ||
                (bytes[i + 3] >> 6) != 0x2) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

std::string ensure(const std::string& text) {
    if (isValid(text)) return text;
#ifdef _WIN32
    int wideLen = MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (wideLen > 0) {
        std::wstring wide(static_cast<std::size_t>(wideLen), L'\0');
        MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), &wide[0], wideLen);
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLen, nullptr, 0, nullptr, nullptr);
        if (utf8Len > 0) {
            std::string out(static_cast<std::size_t>(utf8Len), '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLen, &out[0], utf8Len, nullptr, nullptr);
            return out;
        }
    }
#endif
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text) {
        out += c < 0x80 ? static_cast<char>(c) : '?';
    }
    return out;
}

}
