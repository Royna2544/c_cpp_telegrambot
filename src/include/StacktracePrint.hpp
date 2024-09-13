#pragma once

#include <absl/debugging/stacktrace.h>
#include <absl/debugging/symbolize.h>

#include <array>
#include <functional>
#include <iostream>
#include <sstream>

using stacktrace_foreach_function =
    std::function<bool(const std::string_view &entry)>;

inline void PrintStackTrace(const stacktrace_foreach_function &function) {
    constexpr int kMaxStackDepth = 8;
    std::array<void *, kMaxStackDepth> stack{};
    
    int depth = absl::GetStackTrace(stack.data(), kMaxStackDepth, 0);

    function("Stack trace:");
    for (int i = 0; i < depth; ++i) {
        std::stringstream kLineEntry;

        // Symbolize the stack frame to get the function name and file/line.
        std::array<char, 1024> symbol{};
        kLineEntry << "#" <<  i << " ";
        if (absl::Symbolize(stack[i], symbol.data(), sizeof(symbol))) {
            kLineEntry << symbol.data();
        } else {
            kLineEntry << stack[i];
        }
        if (function(kLineEntry.str())) {
            break;
        }
    }
}