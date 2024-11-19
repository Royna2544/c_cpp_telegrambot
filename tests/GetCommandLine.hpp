#pragma once

#include <vector>

#include "CommandLine.hpp"
#include "gtest/gtest.h"

inline CommandLine getCmdLine() {
    static auto strings = testing::internal::GetArgvs();
    static std::vector<char*> c_strings;

    c_strings.reserve(strings.size() + 1);
    for (auto& s : strings) {
        c_strings.emplace_back(s.data());
    }
    c_strings.emplace_back(nullptr);

    return {static_cast<CommandLine::argc_type>(strings.size()),
            c_strings.data()};
}