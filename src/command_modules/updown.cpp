#include <api/CommandModule.hpp>
#include <api/Providers.hpp>
#include <api/StringResLoader.hpp>
#include <api/TgBotApi.hpp>
#include <filesystem>

DECLARE_COMMAND_HANDLER(up) {
    auto localFile = message->get<MessageAttrs::ExtraText>();
    std::error_code ec;

    if (std::filesystem::exists(localFile, ec)) {
        api->sendReplyDocument(
            message->message(),
            TgBot::InputFile::fromFile(localFile, "application/octet-stream"),
            fmt::format("File: {}\nFile Size: {}B", localFile,
                        std::filesystem::file_size(localFile, ec)));
    } else {
        api->sendReplyMessage(message->message(), res->get(Strings::FAILED_TO_READ_FILE));
    }
}

DECLARE_COMMAND_HANDLER(down) {
    if (!message->reply()->has<MessageAttrs::Document>()) {
        api->sendReplyMessage(message->message(), "Reply to a document");
        return;
    }

    auto fileId = message->reply()->get<MessageAttrs::Document>()->fileId;
    if (!api->downloadFile(message->get<MessageAttrs::ExtraText>(), fileId)) {
        api->sendReplyMessage(message->message(), res->get(Strings::FAILED_TO_DOWNLOAD_FILE));
        return;
    }
    api->sendReplyMessage(message->message(), res->get(Strings::OPERATION_SUCCESSFUL));
}

extern "C" const struct DynModule DYN_COMMAND_EXPORT DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::Enforced,
#ifdef cmd_up_EXPORTS
    .name = "up",
    .description = "Upload designated file from server",
    .function = COMMAND_HANDLER_NAME(up),
    .valid_args =
        {
            .enabled = true,
            .counts = DynModule::craftArgCountMask<1>(),
            .split_type = DynModule::ValidArgs::Split::None,
            .usage = "/up [filename in server]",
        },
#endif
#ifdef cmd_down_EXPORTS
    .name = "down",
    .description = "Download designated file to server",
    .function = COMMAND_HANDLER_NAME(down),
    .valid_args =
        {
            .enabled = true,
            .counts = DynModule::craftArgCountMask<1>(),
            .split_type = DynModule::ValidArgs::Split::None,
            .usage = "/down [filepath to download]",
        },
#endif
};
