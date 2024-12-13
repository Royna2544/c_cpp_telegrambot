#include "sha256.hpp"

SHA256::result_type SHA256::compute(const uint8_t* data, std::size_t length) {
    struct Sha_256 sha_256{};
    result_type hash {};
    sha_256_init(&sha_256, hash.data());
    sha_256_write(&sha_256, data, length);
    sha_256_close(&sha_256);
    return hash;
}