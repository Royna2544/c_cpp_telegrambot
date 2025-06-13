#pragma once

#include <ImgProcExports.h>
#include <absl/status/status.h>

#include <memory>

#include "ImagePBase.hpp"

struct IMGPROC_EXPORT ImageProcessingAll {
    bool read(PhotoBase::Target target);
    absl::Status processAndWrite(const std::filesystem::path& filename);

    explicit ImageProcessingAll(std::filesystem::path filename);
    PhotoBase::Options options;

   private:
    std::filesystem::path _filename;
    std::vector<std::unique_ptr<PhotoBase>> impls;
    std::unique_ptr<PhotoBase> _impl;
};