#pragma once

#include <sstream>
#include <type_traits>

/**
 * @brief Parses the given string into the given output value.
 *
 * @tparam T the type of the output value
 * @param str the string to parse
 * @param outval the output value to fill with the parsed value
 * @return true if the parsing was successful, false otherwise
 */
template <typename String, typename StringView, typename StringStream, typename T>
    requires std::is_trivially_copyable_v<T>
bool _try_parse(const StringView& str, T* outval) {
    StringStream ss(str.data());
    String unused;
    T temp{};

    if (static_cast<bool>(ss >> temp)) {
        if (static_cast<bool>(ss >> unused)) {
            // This means the previous parsing didn't parse full string
            // hence we return false
            return false;
        }
        // We successfully parsed the full string
        *outval = temp;
        return true;
    }
    return false;
}

template <typename String, typename StringView, typename StringStream>
bool _try_parse(const String& str, String* outval) {
    *outval = str;
    return true;
}

template <typename T>
bool try_parse(const std::string_view str, T* outval) {
    return _try_parse<std::string, std::string_view, std::stringstream>(str, outval);
}

template <typename T>
bool try_parse(const std::wstring_view str, T* outval) {
    return _try_parse<std::wstring, std::wstring_view, std::wstringstream>(str, outval);
}