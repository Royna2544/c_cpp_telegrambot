#include <BotReplyMessage.h>
#include <ExtArgs.h>
#include <tgbot/tools/StringTools.h>

#include <MessageWrapper.hpp>
#include <StringResManager.hpp>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <optional>

#include "CommandModule.h"
#include "absl/log/check.h"
#include "tgbot/types/Message.h"

template <DatabaseBase::ListType type>
void handleAddUser(const Bot& bot, const Message::Ptr& message) {
    auto base = TgBotDatabaseImpl::getInstance();
    auto res = base->addUserToList(type, message->replyToMessage->from->id);
    std::string text;
    switch (res) {
        case DatabaseBase::ListResult::OK:
            text = GETSTR(USER_ADDED);
            break;
        case DatabaseBase::ListResult::ALREADY_IN_LIST:
            text = GETSTR(USER_ALREADY_IN_LIST);
            break;
        case DatabaseBase::ListResult::ALREADY_IN_OTHER_LIST:
            text = GETSTR(USER_ALREADY_IN_OTHER_LIST);
            break;
        case DatabaseBase::ListResult::BACKEND_ERROR:
            text = GETSTR(BACKEND_ERROR);
            break;
        default:
            LOG(ERROR) << "Unhandled result type " << static_cast<int>(res);
            text = GETSTR(UNKNOWN_ERROR);
    }
    bot_sendReplyMessage(bot, message, text);
}

template <DatabaseBase::ListType type>
void handleRemoveUser(const Bot& bot, const Message::Ptr& message) {
    auto base = TgBotDatabaseImpl::getInstance();
    auto res =
        base->removeUserFromList(type, message->replyToMessage->from->id);
    std::string text;
    switch (res) {
        case DatabaseBase::ListResult::OK:
            text = GETSTR(USER_REMOVED);
            break;
        case DatabaseBase::ListResult::NOT_IN_LIST:
            text = GETSTR(USER_NOT_IN_LIST);
            break;
        case DatabaseBase::ListResult::ALREADY_IN_OTHER_LIST:
            text = GETSTR(USER_ALREADY_IN_OTHER_LIST);
            break;
        case DatabaseBase::ListResult::BACKEND_ERROR:
            text = GETSTR(BACKEND_ERROR);
            break;
        default:
            LOG(ERROR) << "Unhandled result type " << static_cast<int>(res);
            text = GETSTR(UNKNOWN_ERROR);
    }
    bot_sendReplyMessage(bot, message, text);
}
namespace {

constexpr std::string_view whitelist = "whitelist";
constexpr std::string_view blacklist = "blacklist";
constexpr std::string_view add = "add";
constexpr std::string_view remove = "remove";

void handleDatabaseCmd(const Bot& bot, const Message::Ptr& message) {
    MessageWrapper wrapper(bot, message);

    if (!wrapper.switchToReplyToMessage(GETSTR(REPLY_TO_USER_MSG))) {
        return;
    }

    if (wrapper.hasExtraText()) {
        const auto args = StringTools::split(wrapper.getExtraText(), ' ');
        std::function<void(const Bot&, const Message::Ptr&)> handler;

        wrapper.sendMessageOnExit(GETSTR(INVALID_ARGS_PASSED) + ". " +
                                  GETSTR_IS(EXAMPLE) +
                                  "/database whitelist add");
        if (args.size() == 2) {
            if (args[0] == whitelist) {
                if (args[1] == add) {
                    handler = handleAddUser<DatabaseBase::ListType::WHITELIST>;
                } else if (args[1] == remove) {
                    handler =
                        handleRemoveUser<DatabaseBase::ListType::WHITELIST>;
                }
            } else if (args[0] == blacklist) {
                if (args[1] == add) {
                    handler = handleAddUser<DatabaseBase::ListType::BLACKLIST>;
                } else if (args[1] == remove) {
                    handler =
                        handleRemoveUser<DatabaseBase::ListType::BLACKLIST>;
                }
            }
        }
        if (handler) {
            handler(bot, message);
            wrapper.sendMessageOnExit();
        }
    }
};

void handleSaveIdCmd(const Bot& bot, const Message::Ptr& message) {
    MessageWrapper wrapper(bot, message);
    if (wrapper.hasExtraText()) {
        std::string names = wrapper.getExtraText();
        std::optional<std::string> fileId;
        std::optional<std::string> fileUniqueId;

        if (!wrapper.switchToReplyToMessage(GETSTR(REPLY_TO_GIF_OR_STICKER))) {
            return;
        }

        if (wrapper.hasAnimation()) {
            fileId = wrapper.getAnimation()->fileId;
            fileUniqueId = wrapper.getAnimation()->fileUniqueId;
        } else if (wrapper.hasSticker()) {
            fileId = wrapper.getSticker()->fileId;
            fileUniqueId = wrapper.getSticker()->fileUniqueId;
        } else {
            wrapper.sendMessageOnExit(GETSTR(REPLY_TO_GIF_OR_STICKER));
            return;
        }

        CHECK(fileId && fileUniqueId) << "They should be set";

        DatabaseBase::MediaInfo info{};
        auto const& backend = TgBotDatabaseImpl::getInstance();
        info.mediaId = fileId.value();
        info.mediaUniqueId = fileUniqueId.value();
        info.names = names;

        wrapper.switchToParent();
        if (backend->addMediaInfo(info)) {
            std::stringstream ss;
            ss << "Media with name:" << names;
            wrapper.sendMessageOnExit(ss.str());
        } else {
            wrapper.sendMessageOnExit(GETSTR(MEDIA_ALREADY_IN_DB));
        }
    } else {
        wrapper.sendMessageOnExit(GETSTR(SEND_A_NAME_TO_SAVE));
    }
}

}  // namespace

void loadcmd_database(CommandModule& module) {
    module.command = "database";
    module.description = GETSTR(DATABASE_CMD_DESC);
    module.flags = CommandModule::Flags::Enforced;
    module.fn = handleDatabaseCmd;
}

void loadcmd_saveid(CommandModule& module) {
    module.command = "saveid";
    module.description = GETSTR(SAVEID_CMD_DESC);
    module.flags =
        CommandModule::Flags::Enforced | CommandModule::Flags::HideDescription;
    module.fn = handleSaveIdCmd;
}
