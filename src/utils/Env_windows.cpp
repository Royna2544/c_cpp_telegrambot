#include <Windows.h>

#include <array>
#include <stdexcept>
#include <string_view>

#include "Env.hpp"

const Env::ValueEntry& Env::ValueEntry::operator=(
    const std::string_view value) const noexcept {
    // value.data() is not guaranteed NUL-terminated; copy first.
    const std::string value_str(value);
    SetEnvironmentVariableA(_key.c_str(), value_str.c_str());
    return *this;
}

void Env::ValueEntry::clear() const noexcept {
    SetEnvironmentVariableA(_key.c_str(), nullptr);
}

std::optional<std::string> Env::ValueEntry::get() const noexcept {
    // Query the required size first so values longer than a fixed buffer are
    // not silently truncated.
    const DWORD needed = GetEnvironmentVariableA(_key.c_str(), nullptr, 0);
    if (needed == 0) {
        return std::nullopt;  // not set
    }
    std::string buf(needed, '\0');
    const DWORD written =
        GetEnvironmentVariableA(_key.c_str(), buf.data(), needed);
    if (written == 0) {
        return std::nullopt;
    }
    buf.resize(written);
    return buf;
}