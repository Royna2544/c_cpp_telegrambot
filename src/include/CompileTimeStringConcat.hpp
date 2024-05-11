#pragma once

#include <absl/log/log.h>

#include <string>
#include <string_view>

namespace StringConcat {

// we cannot return a char array from a function, therefore we need a wrapper
template <unsigned N>
struct String {
    char c[N] = {};

    constexpr char operator[](unsigned i) const { return c[i]; }
    
    template <unsigned M>
    constexpr operator String<M>() const {
        String<M> result{};

        static_assert(M > N, "Cannot convert string to smaller buffer");
        for (unsigned i = 0; i < N; i++) {
            result.c[i] = c[i];
        }
        return result;
    }
    String() = default;

    template <unsigned Len>
    explicit constexpr String(const char (&strings)[Len]) {
        static_assert(Len > 2, "Use the char constructor instead");
        static_assert(Len < N, "Cannot convert string to smaller buffer");
        c[Len] = '\0';
        char* dst = c;

        for (const char* src : {strings}) {
            for (; *src != '\0'; src++, dst++) {
                *dst = *src;
            }
        }
    }

    explicit constexpr String(const char l) {
        static_assert(N > 1, "Need at least 2 chars");
        c[0] = l;
        c[1] = '\0';
    }

    [[nodiscard]] constexpr unsigned getSize() const { return N - 1; }

    constexpr operator std::string_view() const {
        return std::string_view(c, N - 1);
    }
    operator std::string() const { return std::string(c, N - 1); }
};

template <unsigned N>
constexpr auto createString(const char (&str)[N]) {
    return String<N + 1>(str);
}

constexpr auto createString(const char (&str)) {
    return String<2>(str);
}

template <unsigned... Len>
consteval auto cat(const char (&... strings)[Len]) {
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

template <unsigned Len>
consteval auto append(const String<Len>& srcBuf, char** dstBuf) {
    int j = 0;
    char* dst = *dstBuf;
    const char* src = srcBuf.c;

    for (; *src != '\0' && j < srcBuf.getSize(); j++, src++, dst++) {
        *dst = *src;
    }
    *dstBuf = dst;
}

template <unsigned... Len>
consteval auto cat(const String<Len>&... strings) {
    constexpr unsigned N = (... + Len) - sizeof...(Len);
    String<N + 1> result = {};
    result.c[N] = '\0';

    char* dst = result.c;

    (append(strings, &dst), ...);
    return result;
}

};  // namespace StringConcat