#include <fmt/core.h>

#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>
#include "api/MessageExt.hpp"

DECLARE_COMMAND_HANDLER(fileid, wrapper, message) {
    std::string file;
    std::string unifile;

    if (message->replyMessage()->exists()) {
        const auto reply = message->replyMessage();
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
            file = unifile = "Unknown";
        }
        wrapper->sendReplyMessage<TgBotApi::ParseMode::Markdown>(
            message->message(),
            fmt::format("FileId: `{}`\nFileUniqueId: `{}`", file, unifile));
    } else {
        wrapper->sendReplyMessage(message->message(), "Reply to a media");
    }
}

DYN_COMMAND_FN(name, module) {
    module.name = "fileid";
    module.description = "Get fileId of a media";
    module.flags = CommandModule::Flags::None;
    module.function = COMMAND_HANDLER_NAME(fileid);
    module.valid_arguments.enabled = true;
    module.valid_arguments.counts.emplace_back(0);
    module.valid_arguments.split_type = CommandModule::ValidArgs::Split::None;
    module.valid_arguments.usage = "<reply-to-a-media>";
    return true;
}