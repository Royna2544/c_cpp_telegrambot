#pragma once

#include <memory>

namespace api::internal {

/**
 * @brief A RecursiveOptional is a wrapper that can hold an optional value of
 * type T. It supports deep copying for types that themselves contain optional values.
 */
template <typename T>
class RecursiveOptional {
    std::unique_ptr<T> ptr;

   public:
    // Constructors
    RecursiveOptional() = default;
    RecursiveOptional(const T& val) : ptr(std::make_unique<T>(val)) {}
    RecursiveOptional(T&& val) : ptr(std::make_unique<T>(std::move(val))) {}

    // 1. Copy Constructor (The Magic: Deep Copy)
    RecursiveOptional(const RecursiveOptional& other) {
        if (other.ptr) ptr = std::make_unique<T>(*other.ptr);
    }

    // 2. Copy Assignment
    RecursiveOptional& operator=(const RecursiveOptional& other) {
        if (this != &other) {
            if (other.ptr)
                ptr = std::make_unique<T>(*other.ptr);
            else
                ptr.reset();
        }
        return *this;
    }

    // Move semantics (Default is fine for unique_ptr)
    RecursiveOptional(RecursiveOptional&&) = default;
    RecursiveOptional& operator=(RecursiveOptional&&) = default;

    // Optional-like API
    bool has_value() const { return ptr != nullptr; }
    T& value() { return *ptr; }
    const T& value() const { return *ptr; }
    void reset() { ptr.reset(); }
    void emplace(const T& val) { ptr = std::make_unique<T>(val); }
    void emplace(T&& val) { ptr = std::make_unique<T>(std::move(val)); }
    explicit operator bool() const { return has_value(); }

    // Pointer syntax support
    T* operator->() { return ptr.get(); }
    const T* operator->() const { return ptr.get(); }
    T& operator*() { return *ptr; }
};

}  // namespace api::internal