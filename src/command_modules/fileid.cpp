#include "CommandModule.h"

static void FileIdCommandFn(const Bot &bot, const Message::Ptr message) {
    const auto replyMsg = message->replyToMessage;
    std::string file, unifile;

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
        } else if (replyMsg->photo.size() == 1) {
            file = replyMsg->photo.front()->fileId;
            unifile = replyMsg->photo.front()->fileUniqueId;
        } else {
            file = unifile = "Unknown";
        }
        bot_sendReplyMessageMarkDown(
            bot, message,
            "FileId: `" + file + "`\n" + "FileUniqueId: `" + unifile + '`');
    } else {
        bot_sendReplyMessage(bot, message, "Reply to a media");
    }
}

void loadcmd_fileid(CommandModule& module) {
    module.command = "fileid";
    module.description = "Get fileId of a media";
    module.flags = CommandModule::Flags::None;
    module.fn = FileIdCommandFn;
}