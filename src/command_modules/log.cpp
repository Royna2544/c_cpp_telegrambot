#include <absl/log/log.h>

#include <LogSinks.hpp>
#include <algorithm>
#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>
#include <chrono>
#include <fstream>

namespace {
constexpr std::uintmax_t kMaximumLogExportBytes = 4U * 1024U * 1024U;

TgBot::InputFile::Ptr readBoundedLogTail(const std::filesystem::path& path) {
    std::error_code error;
    const auto fileSize = std::filesystem::file_size(path, error);
    if (error)
        throw std::runtime_error("cannot inspect the log file");

    const auto bytes = std::min(fileSize, kMaximumLogExportBytes);
    const auto offset = fileSize - bytes;
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot open the log file");
    input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!input)
        throw std::runtime_error("cannot seek the log file");

    auto result = std::make_shared<TgBot::InputFile>();
    if (offset != 0) {
        result->data = "[older log data omitted]\n";
    }
    const auto prefixSize = result->data.size();
    result->data.resize(prefixSize + static_cast<std::size_t>(bytes));
    input.read(result->data.data() + prefixSize,
               static_cast<std::streamsize>(bytes));
    result->data.resize(prefixSize + static_cast<std::size_t>(input.gcount()));
    if (input.bad())
        throw std::runtime_error("cannot read the log file");
    result->mimeType = "text/plain";
    result->fileName = "glider-log-tail.txt";
    return result;
}
}  // namespace

DECLARE_COMMAND_HANDLER(log) {
    auto chat = message->get<MessageAttrs::Chat>();
    const auto source = message->message();
    const auto path = std::filesystem::temp_directory_path() / kDefaultLogFile;
    if (!api->submitCommandWork(
            "log", TgBotApi::WorkClass::Outbound,
            [api, chat = std::move(chat), source, path](std::stop_token stop) {
                if (stop.stop_requested())
                    return;
                try {
                    auto file = readBoundedLogTail(path);
                    if (!stop.stop_requested())
                        api->sendDocument(chat, std::move(file));
                } catch (const std::exception& error) {
                    LOG(ERROR) << "Log export failed: " << error.what();
                    if (!stop.stop_requested())
                        api->sendReplyMessage(source,
                                              "Unable to export the log file.");
                }
            },
            {.deadline = std::chrono::seconds(30)})) {
        api->sendReplyMessage(message->message(),
                              "Log export queue is currently full.");
    }
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::HideDescription | DynModule::Flags::OwnerOnly,
    .name = "log",
    .description = "Get logs",
    .function = COMMAND_HANDLER_NAME(log),
};
