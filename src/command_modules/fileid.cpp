#include <fmt/core.h>

#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>

#include "StringResLoader.hpp"
#include "api/MessageExt.hpp"

DECLARE_COMMAND_HANDLER(fileid) {
    std::string file;
    std::string unifile;

    if (message->reply()->exists()) {
        const auto reply = message->reply();
        if (reply->has<MessageAttrs::Sticker>()) {
            file = reply->get<MessageAttrs::Sticker>()->fileId;
            unifile = reply->get<MessageAttrs::Sticker>()->fileUniqueId;
        } else if (reply->has<MessageAttrs::Animation>()) {
            file = reply->get<MessageAttrs::Animation>()->fileId;
            unifile = reply->get<MessageAttrs::Animation>()->fileUniqueId;
        } else if (reply->has<MessageAttrs::Photo>()) {
            file = reply->get<MessageAttrs::Photo>()->fileId;
            unifile = reply->get<MessageAttrs::Photo>()->fileUniqueId;
        } else {
            file = unifile = access(res, Strings::UNKNOWN);
        }
        api->sendReplyMessage<TgBotApi::ParseMode::Markdown>(
            message->message(),
            fmt::format("FileId: `{}`\nFileUniqueId: `{}`", file, unifile));
    } else {
        api->sendReplyMessage(message->message(),
                                  access(res, Strings::REPLY_TO_A_MEDIA));
    }
}

extern "C" const struct DynModule DYN_COMMAND_EXPORT DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "fileid",
    .description = "Get fileId of a media",
    .function = COMMAND_HANDLER_NAME(fileid),
    .valid_args = {
        .enabled = true,
        .counts = DynModule::craftArgCountMask<0>(),
        .split_type = DynModule::ValidArgs::Split::None,
        .usage = "<reply-to-a-media>",
    }
};
