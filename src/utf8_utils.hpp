#pragma once

#include <string>

namespace utf8 {

bool isValid(const std::string& text);
std::string ensure(const std::string& text);

}
