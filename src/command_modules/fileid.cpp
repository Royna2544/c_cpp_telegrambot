#include <fmt/core.h>

#include "TgBotWrapper.hpp"

DECLARE_COMMAND_HANDLER(fileid, wrapper, message) {
    const auto replyMsg = message->replyToMessage;
    std::string file;
    std::string unifile;

    if (replyMsg) {
        if (replyMsg->sticker) {
            file = replyMsg->sticker->fileId;
            unifile = replyMsg->sticker->fileUniqueId;
        } else if (replyMsg->animation) {
            file = replyMsg->animation->fileId;
            unifile = replyMsg->animation->fileUniqueId;
        } else if (replyMsg->video) {
            file = replyMsg->video->fileId;
            unifile = replyMsg->video->fileUniqueId;
        } else if (replyMsg->photo.size() != 0) {
            file = replyMsg->photo.front()->fileId;
            unifile = replyMsg->photo.front()->fileUniqueId;
        } else {
            file = unifile = "Unknown";
        }
        wrapper->sendReplyMessage<TgBotWrapper::ParseMode::Markdown>(
            message,
            fmt::format("FileId: `{}`\nFileUniqueId: `{}`", file, unifile));
    } else {
        wrapper->sendReplyMessage(message, "Reply to a media");
    }
}

DYN_COMMAND_FN(name, module) {
    module.command = "fileid";
    module.description = "Get fileId of a media";
    module.flags = CommandModule::Flags::None;
    module.function = COMMAND_HANDLER_NAME(fileid);
    return true;
}