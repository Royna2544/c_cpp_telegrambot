#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

extern bool CURL_download_file(const std::string_view url,
                               const std::filesystem::path& where);

extern std::optional<std::string> CURL_download_memory(
    const std::string_view url);
