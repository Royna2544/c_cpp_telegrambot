#pragma once

#include <algorithm>
#include <boost/algorithm/string/split.hpp>
#include <iomanip>
#include <string>
#include <vector>

template <typename T>
auto SingleQuoted(T t) {
    return std::quoted(t, '\'');
}

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
    return str.empty() || std::ranges::all_of(str, [](const char c) {
        return isEmptyChar(c) || isNewline(c);
    });
}

inline void splitWithSpaces(const std::string& str, std::vector<std::string>& out) {
    boost::split(out, str, isEmptyChar);
    std::ranges::remove_if(out, isEmptyOrBlank);
}
