#pragma once

#include <absl/log/log.h>

#include <optional>
#include <tuple>
#include <type_traits>

namespace generic_opt {

// A generic template struct to hold optional data
template <typename T>
struct Option {
    // std::optional to hold the data
    std::optional<T> data;
    T defaultValue;

    // Default constructor (If T is default constructible)
    Option()
        requires std::is_default_constructible_v<T>
        : defaultValue(T{}) {}
    // Default version is a deleted default constructor
    Option() = delete;

    // Option with default value
    explicit Option(T defaultValue) : defaultValue(defaultValue) {}

    template <typename... Args>
        requires std::is_constructible_v<T, Args...>
    explicit Option(Args &&...args)
        : defaultValue(std::forward<Args>(args)...) {}

    // Function to set the data
    void set(T dataIn) { data = dataIn; }
    // Function to reset the data
    void reset() { data.reset(); }

    // Function to get the data and reset it if not persistent
    [[nodiscard]] T get() const {
        // Specially for bool types.
        if constexpr (std::is_same_v<T, bool>) {
            return data.value_or(false);
        }
        if (!operator bool()) {
            DLOG(WARNING) << "Trying to get data which is not set!";
        }
        // Just default value
        return data.value_or(defaultValue);
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
        requires std::is_trivially_constructible_v<
            std::add_lvalue_reference_t<T>, V...>
    {
        set(T{other...});
        return *this;
    }

    // Explicit conversion operator to check if data is present
    explicit operator bool() const { return data.has_value(); }
};
}  // namespace generic_opt