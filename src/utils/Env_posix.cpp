#include <stdexcept>
#include <string_view>

#include "Env.hpp"

const Env::ValueEntry& Env::ValueEntry::operator=(
    const std::string_view value) const noexcept {
    setenv(_key.data(), value.data(), 1);
    return *this;
}

void Env::ValueEntry::clear() const noexcept {
    unsetenv(_key.data());
}

std::optional<std::string> Env::ValueEntry::get() const noexcept {
    const char* value = getenv(_key.data());
    if (value) {
        return std::string(value);
    }
    return std::nullopt;
}