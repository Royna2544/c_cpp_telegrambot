#include "sha256.hpp"

SHA256::result_type SHA256::compute(const uint8_t* data, std::size_t length) {
    result_type result{};
    ::SHA256(data, length, result.data());
    return result;
}