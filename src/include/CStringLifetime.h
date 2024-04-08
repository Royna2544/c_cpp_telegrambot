#pragma once

#include <filesystem>
#include <string>

#include "CompileTimeStringConcat.hpp"

/**
 * @brief A helper class that manages the lifetime of a C-style string
 *
 * This class is a simple wrapper around a std::string that ensures that the
 * underlying C-style string is always valid. It does this by managing the
 * lifetime of the std::string object and ensuring that the C-style string is
 * always up-to-date.
 *
 * This class can be used to simplify the creation and management of C-style
 * strings in a way that is safe and memory-efficient.
 *
 * @tparam TAllocator The allocator to use for the std::string object
 */
class CStringLifetime {
   public:
    /**
     * @brief Default constructor
     *
     * The default constructor creates an empty CStringLifetime object.
     */
    CStringLifetime() = default;

    /**
     * @brief Construct a CStringLifetime object from a std::string
     *
     * The constructor takes a std::string object and wraps it in a
     * CStringLifetime object. The C-style string is managed by the
     * CStringLifetime object and will be updated if the std::string object is
     * modified.
     *
     * @param other The std::string object to wrap
     */
    CStringLifetime(const std::string& other) : _str(other) {
        _c_str = _str.c_str();
    }

    /**
     * @brief Construct a CStringLifetime object from a rvalue std::string
     *
     * The constructor takes a rvalue std::string object and wraps it in a
     * CStringLifetime object. The C-style string is managed by the
     * CStringLifetime object and will be updated if the std::string object is
     * modified.
     *
     * @param other The rvalue std::string object to wrap
     */
    CStringLifetime(std::string&& other) : _str(std::move(other)) {
        _c_str = _str.c_str();
    }

    /**
     * @brief Copy constructor
     *
     * The copy constructor creates a new CStringLifetime object that is a copy
     * of an existing CStringLifetime object. The C-style string is managed by
     * the new CStringLifetime object and will be updated if the original
     * CStringLifetime object is modified.
     *
     * @param other The CStringLifetime object to copy
     */
    CStringLifetime(const CStringLifetime& other) : _str(other._str) {
        _c_str = _str.c_str();
    }

    // Takes std::filesystem::path
    CStringLifetime(const std::filesystem::path path)
        : CStringLifetime(path.string()) {}

    // Takes string literal
    template <unsigned N>
    CStringLifetime(const StringConcat::String<N> other)
        : CStringLifetime(static_cast<std::string>(other)) {}

    // Takes string literal
    CStringLifetime(const char* other) : CStringLifetime(std::string(other)) {}

    /**
     * @brief Move constructor
     *
     * The move constructor creates a new CStringLifetime object that takes
     * ownership of an existing CStringLifetime object. The C-style string is
     * managed by the new CStringLifetime object and will be updated if the
     * original CStringLifetime object is modified.
     *
     * @param other The CStringLifetime object to move
     */
    CStringLifetime(CStringLifetime&& other) noexcept
        : _str(std::move(other._str)) {
        _c_str = _str.c_str();
    }

    /**
     * @brief Assignment operator
     *
     * The assignment operator assigns a new value to a CStringLifetime object.
     * If the new value is a std::string, the C-style string is updated. If the
     * new value is another CStringLifetime object, the C-style string is
     * updated to reflect the contents of the other object.
     *
     * @param other The new value to assign
     * @return CStringLifetime& A reference to the updated CStringLifetime
     * object
     */
    CStringLifetime& operator=(const std::string& other) {
        onNewStr(other);
        return *this;
    }

    /**
     * @brief Assignment operator
     *
     * The assignment operator assigns a new value to a CStringLifetime object.
     * If the new value is a rvalue std::string, the C-style string is updated.
     * If the new value is another CStringLifetime object, the C-style string is
     * updated to reflect the contents of the other object.
     *
     * @param other The new value to assign
     * @return CStringLifetime& A reference to the updated CStringLifetime
     * object
     */
    CStringLifetime& operator=(std::string&& other) {
        onNewStr(std::move(other));
        return *this;
    }

    /**
     * @brief Assignment operator
     *
     * The assignment operator assigns a new value to a CStringLifetime object.
     * If the new value is a CStringLifetime object, the C-style string is
     * updated to reflect the contents of the other object.
     *
     * @param other The new value to assign
     * @return CStringLifetime& A reference to the updated CStringLifetime
     * object
     */
    CStringLifetime& operator=(const CStringLifetime& other) {
        onNewStr(other._str);
        return *this;
    }

    /**
     * @brief Get the C-style string
     *
     * This function returns a pointer to the C-style string managed by the
     * CStringLifetime object. The pointer is guaranteed to be valid as long as
     * the CStringLifetime object is in scope.
     *
     * @return const char* A pointer to the C-style string
     */
    const char* get() const { return _c_str; }

    operator const char*() const { return get(); }

   private:
    /**
     * @brief Update the C-style string with a new value
     *
     * This function updates the C-style string with a new value. If the new
     * value is a std::string, the C-style string is updated to reflect the
     * contents of the std::string. If the new value is another
     * CStringLifetime object, the C-style string is updated to reflect the
     * contents of the other object.
     *
     * @param other The new value to use
     */
    void onNewStr(const std::string& other) {
        _str = other;
        _c_str = _str.c_str();
    }

    /**
     * @brief Update the C-style string with a new value
     *
     * This function updates the C-style string with a new value. If the new
     * value is a rvalue std::string, the C-style string is updated to reflect
     * the contents of the std::string. If the new value is another
     * CStringLifetime object, the C-style string is updated to reflect the
     * contents of the other object.
     *
     * @param other The new value to use
     */
    void onNewStr(std::string&& other) {
        _str = std::move(other);
        _c_str = _str.c_str();
    }

    std::string _str;
    const char* _c_str = nullptr;
};