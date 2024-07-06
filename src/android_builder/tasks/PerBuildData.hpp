#pragma once

#include <ConfigParsers.hpp>
#include <absl/log/check.h>

struct PerBuildData {
    enum class Result { SUCCESS, ERROR_NONFATAL, ERROR_FATAL };
    BuildConfig bConfig;
    RomConfig rConfig;
    std::filesystem::path scriptDirectory;
    struct ResultData {
        Result value{};
        std::array<char, 250> msg{};
        void setMessage(const std::string& message) {
            CHECK(message.size() < msg.size());
            std::strncpy(msg.data(), message.c_str(), msg.size() - 1);
        }
        [[nodiscard]] std::string getMessage() const noexcept {
            return msg.data();
        }
    } *result;
};
