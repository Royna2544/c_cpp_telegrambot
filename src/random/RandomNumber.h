#pragma once

/**
 * genRandomNumber - Generate random number given a range.
 * Conditionally uses platform-specific RNG.
 *
 * @param min min value
 * @param max max value
 * @throws std::runtime_error if min >= max
 * @return Generated number
 */
int genRandomNumber(const int min, const int max);

/**
 * Alias for genRandomNumber(int, int) with min parameter as 0
 * @param max max value
 *
 * @throws std::runtime_error if min >= max
 * @return Generated number
 */
int genRandomNumber(const int max);
