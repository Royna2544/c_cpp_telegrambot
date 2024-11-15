#pragma once

#include <absl/strings/str_cat.h>
#include <trivial_helpers/_class_helper_macros.h>

#include <string>
#include <string_view>
#include <TgBotUtilsExports.h>

// A C++-like interface for manipulating environment variables.
class TgBotUtils_API Env {
   public:
    Env() = default;

    class ValueEntry {
        std::string _key;

       public:
        explicit ValueEntry(const std::string_view key) : _key(key) {}
        // Aka, setenv
        const Env::ValueEntry& operator=(const std::string_view value) const;
        // Aka, unsetenv
        void clear() const;
        // Aka, getenv
        [[nodiscard]] std::string get() const;

        [[nodiscard]] bool has() const;

        bool assign(std::string& ref) const {
            if (has()) {
                ref = get();
                return true;
            }
            return false;
        }
        std::string_view key() const {
            return _key;
        }

        ValueEntry& operator=(const ValueEntry& other) {
            if (this == &other) {
                return *this;
            }
            *this = other.get();
            return *this;
        }

        // This is a ValueEntry, we are only playing with values...
        ValueEntry& operator=(ValueEntry&& other) {
            *this = std::move(other.get());
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
        const Env::ValueEntry& operator+=(const std::string_view addition) const {
            *this = absl::StrCat(get(), addition);
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
    if (entry.has()) {
        o << entry.get();
    } else {
        o << "(nonexistent variable " <<  entry.key() << ")";
    }
    return o;
}