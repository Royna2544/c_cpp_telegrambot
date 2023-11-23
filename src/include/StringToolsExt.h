#pragma once

#include <algorithm>
#include <string>

static inline bool isEmptyOrBlank(const std::string& str) {
    return str.empty() || std::all_of(str.begin(), str.end(),
                                      [](char c) { return std::isspace(c); });
}
