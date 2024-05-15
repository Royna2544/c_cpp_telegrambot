#pragma once

#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <string>

static inline bool isEmptyChar(const char c) {
    return std::isspace(c) || c == 0;
}

static inline bool isEmptyOrBlank(const std::string& str) {
    return str.empty() || std::all_of(str.begin(), str.end(), isEmptyChar);
}

static inline void TrimStr(std::string& str) { boost::trim(str); }

#if __has_include(<tgbot/tools/StringTools.h>)
#include <tgbot/tools/StringTools.h>

#include <vector>

static inline void splitAndClean(const std::string& text,
                                 std::vector<std::string>& out, char delimiter='\n') {
    auto v = StringTools::split(text, delimiter);
    auto it = std::remove_if(v.begin(), v.end(), [](const std::string& s) {
        return isEmptyOrBlank(s);
    });
    v.erase(it, v.end());
    out = v;
}
#endif