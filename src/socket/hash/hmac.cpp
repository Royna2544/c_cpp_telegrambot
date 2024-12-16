#include "hmac.hpp"

#include <openssl/hmac.h>

HMAC::result_type HMAC::compute(const uint8_t* data, std::size_t length,
                                const std::string_view key) {
    // Buffer to store the HMAC result
    result_type hmac_result{};
    unsigned int hmac_length = 0;

    // Compute the HMAC
    ::HMAC(EVP_sha256(), key.data(), key.length(), data, length,
           hmac_result.data(), &hmac_length);

    if (hmac_length != EVP_MAX_MD_SIZE) {
        for (unsigned int i = hmac_length; i < EVP_MAX_MD_SIZE; i++) {
            hmac_result[i] = 0;  // Pad with zeros if necessary
        }
    }
    return hmac_result;
}