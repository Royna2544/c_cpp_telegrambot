#pragma once

#include <SharedMalloc.hpp>
#include <optional>
#include <type_traits>

struct SocketData {
    using length_type = int64_t;

    std::optional<SharedMalloc> data;
    length_type len;

    // Constructor that takes malloc
    template <typename T>
        requires(!std::is_integral_v<T>)
    explicit SocketData(T in_data) : len(sizeof(in_data)), data(len) {
        data = SharedMalloc(len);
        memcpy(data->getData(), &in_data, len);
    }
    explicit SocketData(length_type len) : len(len), data(len) {
        data = SharedMalloc(len);
    }
};
