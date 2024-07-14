#pragma once

#include <string>
#include <type_traits>
#include <vector>

#include <TgBotRandomExports.h>

// Retval type for random
using random_return_type = long;
static_assert(std::is_integral_v<random_return_type>);

namespace RandomNumberGenerator {

/**
 * generate - Generate random number given a range.
 * Conditionally uses platform-specific RNG.
 *
 * @param min min value
 * @param max max value
 * @throws std::runtime_error if min >= max
 * @return Generated number
 */
random_return_type generate(const random_return_type min,
                            const random_return_type max);

/**
 * Alias for genRandomNumber(int, int) with min parameter as 0
 * @param max max value
 *
 * @throws std::runtime_error if min >= max
 * @return Generated number
 */
random_return_type generate(const random_return_type max);

/**
 * Template function to shuffle an array of elements.
 * This function is specialized for std::string type to call an external
 * function. For other types, it is deleted to prevent compilation errors.
 *
 * @tparam Elem The type of elements in the vector.
 * @param in The vector of elements to be shuffled.
 *
 * @note This function is marked as = delete for all types except std::string.
 *       For std::string, it calls an external function shuffleStringArray.
 *
 * @throws No exceptions are thrown by this function.
 */
template <typename Elem>
void shuffleArray(std::vector<Elem>& in) = delete;

/**
 * Specialization of shuffleArray for std::string type.
 * Calls an external function shuffleStringArray to shuffle the vector of
 * strings.
 *
 * @param in The vector of strings to be shuffled.
 *
 * @note This function is called only when the template parameter Elem is
 * std::string. It calls an external function shuffleStringArray.
 *
 * @throws No exceptions are thrown by this function.
 */
template <>
inline void shuffleArray<std::string>(std::vector<std::string>& inArray) {
    extern void shuffleStringArray(std::vector<std::string> & inArray);
    shuffleStringArray(inArray);
}

};  // namespace RandomNumberGenerator
