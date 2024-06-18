#pragma once

#include <algorithm>
#include <string>

inline bool isNewline(const char c) {
    return c == '\n' || c == '\r';
}

inline bool isWhitespace(const char c) {
    return static_cast<bool>(std::isspace(c));
}

inline bool isEmptyChar(const char c) {
    return isWhitespace(c) || c == 0;
}

inline bool isEmptyOrBlank(const std::string& str) {
    return str.empty() || std::ranges::all_of(str, isEmptyChar);
}
