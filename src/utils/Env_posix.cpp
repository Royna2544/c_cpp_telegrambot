#include <stdexcept>
#include <string_view>

#include "Env.hpp"

const Env::ValueEntry& Env::ValueEntry::operator=(const std::string_view value) const {
    setenv(_key.data(), value.data(), 1);
    return *this;
}

void Env::ValueEntry::clear() const { unsetenv(_key.data()); }

std::string Env::ValueEntry::get() const {
    if (!has()) {
        throw std::invalid_argument("env variable not set: " + _key);
    }
    return getenv(_key.data());
}

bool Env::ValueEntry::has() const {
    char *env = getenv(_key.data());
    return env != nullptr;
}
