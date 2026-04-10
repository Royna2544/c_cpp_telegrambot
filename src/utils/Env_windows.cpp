#include <Windows.h>

#include <array>
#include <stdexcept>
#include <string_view>

#include "Env.hpp"

const Env::ValueEntry& Env::ValueEntry::operator=(
    const std::string_view value) const noexcept {
    SetEnvironmentVariableA(_key.data(), value.data());
    return *this;
}

void Env::ValueEntry::clear() const noexcept {
    SetEnvironmentVariableA(_key.data(), nullptr);
}

std::optional<std::string> Env::ValueEntry::get() const noexcept {
    std::array<char, 1024> buf = {};
    if (GetEnvironmentVariableA(_key.data(), buf.data(), buf.size() - 1) != 0) {
        return buf.data();
    }
    return std::nullopt;
}