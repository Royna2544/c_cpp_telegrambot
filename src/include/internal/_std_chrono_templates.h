#pragma once

#include <chrono>

using std::chrono_literals::operator""s;

template <class Dur>
std::chrono::hours to_hours(Dur &&it) {
    return std::chrono::duration_cast<std::chrono::hours>(it);
}

template <class Dur>
std::chrono::minutes to_mins(Dur &&it) {
    return std::chrono::duration_cast<std::chrono::minutes>(it);
}

template <class Dur>
std::chrono::seconds to_secs(Dur &&it) {
    return std::chrono::duration_cast<std::chrono::seconds>(it);
}

template <class Dur>
std::chrono::milliseconds to_msecs(Dur &&it) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(it);
}
