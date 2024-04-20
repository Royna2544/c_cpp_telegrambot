#pragma once

#include <sstream>

/**
 * @brief Parses the given string into the given output value.
 * 
 * @tparam T the type of the output value
 * @param str the string to parse
 * @param outval the output value to fill with the parsed value
 * @return true if the parsing was successful, false otherwise
 */
template <typename T>
bool try_parse(const std::string str, T* outval) {
    return static_cast<bool>(std::stringstream(str) >> *outval);
}