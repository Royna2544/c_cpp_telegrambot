#include <stdexcept>
#include <string_view>

#include "Env.hpp"

const Env::ValueEntry& Env::ValueEntry::operator=(
    const std::string_view value) const noexcept {
    // value.data() is not guaranteed NUL-terminated; copy into a std::string
    // before handing it to the C API.
    const std::string value_str(value);
    setenv(_key.c_str(), value_str.c_str(), 1);
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