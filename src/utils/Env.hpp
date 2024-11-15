#pragma once

#include <trivial_helpers/_class_helper_macros.h>

#include <stdexcept>
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
        void operator=(const std::string_view value) const;
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

        // Do not allow copying of this element outside the function use.
        NO_COPY_CTOR(ValueEntry);
        NO_MOVE_CTOR(ValueEntry);
        ValueEntry() = delete;
        ~ValueEntry() = default;
    };

    ValueEntry operator[](const std::string_view key) const {
        return ValueEntry{key};
    }
};
