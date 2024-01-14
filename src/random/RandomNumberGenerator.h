#pragma once

#include <string>
#include <type_traits>
#include <vector>

// Retval type for random
using random_return_type = long;
static_assert(std::is_integral_v<random_return_type>);

/**
 * genRandomNumber - Generate random number given a range.
 * Conditionally uses platform-specific RNG.
 *
 * @param min min value
 * @param max max value
 * @throws std::runtime_error if min >= max
 * @return Generated number
 */
random_return_type genRandomNumber(const random_return_type min, const random_return_type max);

/**
 * Alias for genRandomNumber(int, int) with min parameter as 0
 * @param max max value
 *
 * @throws std::runtime_error if min >= max
 * @return Generated number
 */
random_return_type genRandomNumber(const random_return_type max);

/**
 * shuffleStringArray - Shuffle a string vector
 * Conditionally uses platform-specific RNG.
 *
 * @param in in vector
 * @return void
 */
void shuffleStringArray(std::vector<std::string>& in);
