#include <winbase.h>

#include <array>
#include <stdexcept>
#include <string_view>

#include "Env.hpp"

void Env::ValueEntry::operator=(const std::string_view value) const {
    SetEnvironmentVariableA(_key.data(), value.data());
}

void Env::ValueEntry::clear() const {
    SetEnvironmentVariableA(_key.data(), nullptr);
}

std::string Env::ValueEntry::get() const {
    std::array<char, 1024> buf = {};
    if (GetEnvironmentVariableA(_key.data(), buf.data(), buf.size()) != 0) {
        return buf.data();
    }
    throw std::invalid_argument("env variable not set: " + _key);
}

bool Env::ValueEntry::has() const {
    try {
        (void)get();
        return true;
    } catch (const std::invalid_argument& ex) {
        return false;
    }
}
