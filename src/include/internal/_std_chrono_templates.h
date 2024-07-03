#pragma once

#include <chrono>
#include <iomanip>
#include <string>
#include <sstream>

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

template <class Dur>
std::string to_string(const Dur out) {
    const auto hms = std::chrono::hh_mm_ss(out);
    std::stringstream ss;

    ss << hms.hours().count() << "h " << hms.minutes().count() << "m "
       << hms.seconds().count() << "s";
    return ss.str();
}

union Time_t {
    time_t val;
};

inline std::ostream& operator<<(std::ostream& self, const Time_t tp) {
    self << std::put_time(std::gmtime(&tp.val), "%Y-%m-%dT%H:%M:%SZ (GMT)");
    return self;
}

template <class T>
Time_t fromTP(std::chrono::time_point<T> it) {
    return {std::chrono::system_clock::to_time_t(it)};
}