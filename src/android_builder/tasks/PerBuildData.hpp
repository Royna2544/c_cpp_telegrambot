#pragma once

#include <absl/log/check.h>

#include <ConfigParsers.hpp>

struct PerBuildData {
    enum class Result { SUCCESS, ERROR_NONFATAL, ERROR_FATAL };
    BuildConfig bConfig;
    RomConfig rConfig;
    std::filesystem::path scriptDirectory;
    struct ResultData {
        static constexpr int MSG_SIZE = 250;
        Result value{};
        std::array<char, MSG_SIZE> msg{};
        void setMessage(const std::string& message) {
            LOG_IF(WARNING, message.size() > msg.size())
                << "Message size is " << message.size()
                << " bytes, which exceeds limit";
            std::strncpy(msg.data(), message.c_str(), msg.size() - 1);
        }
        [[nodiscard]] std::string getMessage() const noexcept {
            return msg.data();
        }
    } *result;
};
