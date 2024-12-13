#pragma once

#include <array>
#include <cstdint>

#include "sha-2/sha-256.h"

class SHA256 {
   public:
    using result_type = std::array<uint8_t, SIZE_OF_SHA_256_HASH>;

    static result_type compute(const uint8_t* data, std::size_t length);
};