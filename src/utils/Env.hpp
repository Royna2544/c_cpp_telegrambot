#pragma once

#include <UtilsExports.h>
#include <absl/log/log.h>
#include <absl/strings/str_cat.h>
#include <trivial_helpers/_class_helper_macros.h>

#include <concepts>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>

// A C++-like interface for manipulating environment variables.
class UTILS_EXPORT Env {
   public:
    Env() = default;

    class UTILS_EXPORT ValueEntry {
        std::string _key;

       public:
        explicit ValueEntry(const std::string_view key) : _key(key) {}
        // Aka, setenv
        const Env::ValueEntry& operator=(
            const std::string_view value) const noexcept;
        // Aka, unsetenv
        void clear() const noexcept;
        // Aka, getenv
        [[nodiscard]] std::optional<std::string> get() const noexcept;

        template <typename T>
            requires std::is_assignable_v<std::string, T>
        bool assign(T& ref) const {
            auto value = get();
            if (value) {
                ref = *value;
                return true;
            }
            DLOG(WARNING) << "env name=" << _key << " is not set";
            return false;
        }
        std::string_view key() const { return _key; }

        ValueEntry& operator=(const ValueEntry& other) {
            if (this == &other) {
                return *this;
            }
            // Get the value from the other entry and set it to this entry.
            auto value = other.get();
            if (value.has_value()) {
                *this = value.value();
            } else {
                // If the other entry doesn't have a value, clear this entry.
                clear();
            }
            return *this;
        }

        // This is a ValueEntry, we are only playing with values...
        ValueEntry& operator=(ValueEntry&& other) noexcept {
            operator=(other);
            other.clear();
            return *this;
        }

        bool operator==(const std::string_view other) const {
            return get() == other;
        }

        // Append a string to the current value.
        // Note: This will not overwrite the existing value.
        // To overwrite, use operator=
        // When get() throws, this won't catch it.
        const Env::ValueEntry& operator+=(
            const absl::string_view addition) const {
            auto current = get();
            if (!current.has_value()) {
                *this = addition;
                return *this;
            }
            *this = absl::StrCat(current.value(), addition);
            return *this;
        }

        // Remove the suffix if the current value ends with it.
        const Env::ValueEntry& operator-=(
            const absl::string_view suffix) const {
            auto current = get();
            if (!current.has_value()) {
                return *this;
            }
            if (current->size() >= suffix.size() &&
                current->compare(current->size() - suffix.size(), suffix.size(),
                                suffix) == 0) {
                *this = current->substr(0, current->size() - suffix.size());
            }
            return *this;
        }

        ValueEntry() = delete;
        ~ValueEntry() = default;
    };

    ValueEntry operator[](const std::string_view key) const {
        return ValueEntry{key};
    }
};

inline std::ostream& operator<<(std::ostream& o, const Env::ValueEntry& entry) {
    auto value = entry.get();
    if (value) {
        o << *value;
    } else {
        o << "(not set)";
    }
    return o;
}
