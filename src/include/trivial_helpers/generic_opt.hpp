#pragma once

#include <absl/log/log.h>

#include <optional>
#include <type_traits>

namespace generic_opt {

// A generic template struct to hold optional data
template <typename T>
struct Option {
    // std::optional to hold the data
    std::optional<T> data;

    // Default constructor
    Option() = default;

    // Option with default value
    explicit Option(T defaultValue) : data(defaultValue) {}

    // Function to set the data
    void set(T dataIn) { data = dataIn; }

    // Function to get the data and reset it if not persistent
    [[nodiscard]] T get() const {
        // Specially for bool types.
        if constexpr (std::is_same_v<T, bool>) {
            return data.value_or(false);
        }
        if (!operator bool()) {
            LOG(WARNING) << "Trying to get data which is not set!";
        }
        // Throws std::bad_optional_access if not set
        return data.value();
    }

    template <typename V>
    Option &operator=(const V &other)
        requires std::is_assignable_v<std::add_lvalue_reference_t<T>, V>
    {
        set(other);
        return *this;
    }
    template <typename... V>
    Option &operator=(const V &...other)
        requires std::is_trivially_constructible_v<std::add_lvalue_reference_t<T>, V...>
    {
        set(T{other...});
        return *this;
    }


    // Explicit conversion operator to check if data is present
    explicit operator bool() const { return data.has_value(); }
};
}  // namespace generic_opt