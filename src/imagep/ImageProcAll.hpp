#pragma once

#include <ImgProcExports.h>

#include <chrono>
#include <memory>
#include <stop_token>

#include "ImagePBase.hpp"

struct IMGPROC_EXPORT ImageProcessingAll {
    bool read(PhotoBase::Target target, std::stop_token stop = {},
              std::chrono::steady_clock::time_point deadline =
                  std::chrono::steady_clock::time_point::max());
    PhotoBase::TinyStatus processAndWrite(
        const std::filesystem::path& filename, std::stop_token stop = {},
        std::chrono::steady_clock::time_point deadline =
            std::chrono::steady_clock::time_point::max());

    explicit ImageProcessingAll(std::filesystem::path filename);
    PhotoBase::Options options;

   private:
    std::filesystem::path _filename;
    std::vector<std::unique_ptr<PhotoBase>> impls;
    std::unique_ptr<PhotoBase> _impl;
};
