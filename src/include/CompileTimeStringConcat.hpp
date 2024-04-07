#pragma once

#include <string>
#include <string_view>

namespace StringConcat {

// we cannot return a char array from a function, therefore we need a wrapper
template <unsigned N>
struct String {
    char c[N];
    operator std::string_view() const { return std::string_view(c, N - 1); }
    operator std::string() const { return std::string(c, N - 1); }
};

template <unsigned... Len>
constexpr auto cat(const char (&... strings)[Len]) {
    constexpr unsigned N = (... + Len) - sizeof...(Len);
    String<N + 1> result = {};
    result.c[N] = '\0';

    char* dst = result.c;
    for (const char* src : {strings...}) {
        for (; *src != '\0'; src++, dst++) {
            *dst = *src;
        }
    }
    return result;
}

};  // namespace StringConcat