#pragma once

#include <array>
#include <cstdint>

class CRC32 {
    static constexpr unsigned int GENERATOR_POLYNOMIAL =
        0b00000100110000010001110110110111;
    static constexpr int TABLE_LENGTH = 256;

    static constexpr std::array<uint32_t, TABLE_LENGTH> generateCRCTable();

   public:
    using result_type = uint32_t;

    static result_type compute(const uint8_t* data, std::size_t length);
};
