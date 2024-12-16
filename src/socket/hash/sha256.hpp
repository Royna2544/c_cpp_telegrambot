#pragma once

#include <array>
#include <cstdint>

#include <openssl/sha.h>

class SHA256 {
   public:
    using result_type = std::array<uint8_t, SHA256_DIGEST_LENGTH>;

    static result_type compute(const uint8_t* data, std::size_t length);
};