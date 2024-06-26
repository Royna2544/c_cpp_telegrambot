#include <filesystem>
#include "absl/base/log_severity.h"

constexpr int MAX_LOGMSG_SIZE = 1024;
constexpr auto LOGMSG_MAGIC = 0xAABBCCDD;

inline std::filesystem::path getSocketPathForLogging() {
    static auto path = std::filesystem::temp_directory_path() / "tgbot_log.sock";
    return path;
}

struct LogEntry {
    uint64_t magic = LOGMSG_MAGIC;
    absl::LogSeverity severity {};
    std::array<char, MAX_LOGMSG_SIZE> message{};
};