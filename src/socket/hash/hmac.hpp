#pragma once

#include <openssl/evp.h>

#include <array>
#include <cstdint>
#include <string_view>

class HMAC {
   public:
    using result_type = std::array<unsigned char, EVP_MAX_MD_SIZE>;

    static result_type compute(const uint8_t* data, std::size_t length,
                               const std::string_view key);
};
