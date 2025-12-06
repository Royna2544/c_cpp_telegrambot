#pragma once

#include <sstream>
#include <type_traits>

namespace details::string {

/**
 * @brief Parses the given string into the given output value.
 *
 * @tparam T the type of the output value
 * @param str the string to parse
 * @param outval the output value to fill with the parsed value
 * @return true if the parsing was successful, false otherwise
 */
template <typename String, typename StringView, typename StringStream,
          typename T>
    requires std::is_trivially_copyable_v<T>
bool try_parse(const StringView& str, T* outval) {
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
bool try_parse(const String& str, String* outval) {
    *outval = str;
    return true;
}

}  // namespace details::string

template <typename T>
bool try_parse(const std::string_view str, T* outval) {
    return details::string::try_parse<std::string, std::string_view,
                                      std::stringstream>(str, outval);
}

template <typename T>
bool try_parse(const std::wstring_view str, T* outval) {
    return details::string::try_parse<std::wstring, std::wstring_view,
                                      std::wstringstream>(str, outval);
}

// Specialization for const char* when we have different types
// const char* could be converted to both stringview types.
// So explicitly give a function taking const char*
template <typename Str, typename T>
bool try_parse(const Str str, T* outval)
    requires(std::is_assignable_v<std::string_view, Str> &&
             !std::is_same_v<Str, std::string_view>)
{
    return details::string::try_parse<std::string, std::string_view,
                                      std::stringstream>(str, outval);
}
