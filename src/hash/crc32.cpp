#include "crc32.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>

template <typename IntT, std::enable_if_t<std::is_unsigned_v<IntT>, bool> = true>
constexpr IntT reverseBits(IntT value) {
    IntT result = 0;
    for (int i = 0; i < std::numeric_limits<IntT>::digits; ++i) {
        result |= ((value >> i) & 1)
                  << (std::numeric_limits<IntT>::digits - 1 - i);
    }
    return result;
}

constexpr std::array<uint32_t, CRC32::TABLE_LENGTH> CRC32::generateCRCTable() {
    constexpr uint32_t REVERSED_GENERATOR_POLYNOMIAL =
        reverseBits(GENERATOR_POLYNOMIAL);
    std::array<uint32_t, CRC32::TABLE_LENGTH> table = {};
    for (uint32_t i = 0; i < CRC32::TABLE_LENGTH; ++i) {
        uint32_t crc = i;
        for (int j = 0; j < std::numeric_limits<uint8_t>::digits; ++j) {
            if ((crc & 1) != 0) {
                crc = (crc >> 1) ^ REVERSED_GENERATOR_POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
        table[i] = crc;
    }
    return table;
}

uint32_t CRC32::compute(const uint8_t* data, std::size_t length) {
    static constexpr std::array<uint32_t, TABLE_LENGTH> crcTable =
        generateCRCTable();
    uint32_t crc = ~0U;
    while ((length--) != 0) {
        crc = (crc >> std::numeric_limits<uint8_t>::digits) ^
              crcTable[(crc ^ *data++) & std::numeric_limits<uint8_t>::max()];
    }
    return ~crc;
}
