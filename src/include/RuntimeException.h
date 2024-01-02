#pragma once

#include <cstdio>
#include <stdexcept>

template <class... F>
std::runtime_error runtime_errorf(const char* fmt, F... args) {
    char buf[1024] = {0};
    snprintf(buf, sizeof(buf) - 1, fmt, args...);
    return std::runtime_error(buf);
}
