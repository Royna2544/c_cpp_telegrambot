#pragma once

#include <filesystem>
#include "trivial_helpers/raii.hpp"

class Tar {
    RAII<struct archive*>::Value<int> _archive;
    constexpr static int blocksize = 10240;

   public:
    explicit Tar(const std::filesystem::path& path);
    Tar() = default;
    ~Tar();

    bool open(const std::filesystem::path& path);
    bool extract(const std::filesystem::path& path) const;
};