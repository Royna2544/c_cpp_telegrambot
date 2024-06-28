#include <BotReplyMessage.h>
#include <absl/log/check.h>
#include <internal/_tgbot.h>
#include <tgbot/tgbot.h>

#include <MessageWrapper.hpp>
#include <OnAnyMessageRegister.hpp>
#include <StringResManager.hpp>
#include <boost/algorithm/string/split.hpp>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <memory>
#include <optional>

#include "CommandModule.h"
#include "StringToolsExt.hpp"
#include "random/RandomNumberGenerator.h"

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

using TgBot::KeyboardButton;

std::vector<std::vector<KeyboardButton::Ptr>> genKeyboard(const int total,
                                                          const int x) {
    std::vector<std::vector<KeyboardButton::Ptr>> keyboard;
    int yrow = total / x;
    if (total % x != 0) {
        yrow += 1;
    }
    keyboard.reserve(yrow);
    for (int i = 0; i < yrow; i++) {
        std::vector<KeyboardButton::Ptr> row;
        row.reserve(x);
        for (int j = 0; j < x; j++) {
            row.emplace_back(std::make_shared<KeyboardButton>());
        }
        keyboard.push_back(row);
    }
    return keyboard;
}

constexpr std::string_view addtowhitelist = "Add to whitelist";
constexpr std::string_view removefromwhitelist = "Remove from whitelist";
constexpr std::string_view addtoblacklist = "Add to blacklist";
constexpr std::string_view removefromblacklist = "Remove from blacklist";

void handleDatabaseCmd(Bot& bot, const Message::Ptr& message) {
    MessageWrapper wrapper(bot, message);

    auto reply = std::make_shared<TgBot::ReplyKeyboardMarkup>();
    reply->keyboard = genKeyboard(4, 2);
    reply->keyboard[0][0]->text = addtowhitelist;
    reply->keyboard[0][1]->text = removefromwhitelist;
    reply->keyboard[1][0]->text = addtoblacklist;
    reply->keyboard[1][1]->text = removefromblacklist;
    reply->oneTimeKeyboard = true;
    reply->resizeKeyboard = true;

    if (!wrapper.switchToReplyToMessage(GETSTR(REPLY_TO_USER_MSG))) {
        return;
    }

    auto msg = bot.getApi().sendMessage(
        message->chat->id,
        "Choose what u want to do with " +
            UserPtr_toString(message->replyToMessage->from),
        nullptr, nullptr,

        reply);

    const auto registerer = OnAnyMessageRegisterer::getInstance();
    const random_return_type token = RandomNumberGenerator::generate(100);

    registerer->registerCallback(
        [msg, registerer, token](const Bot& bot, Message::Ptr m) {
            if (m->replyToMessage->messageId == msg->messageId) {
                if (m->text == addtowhitelist) {
                    handleAddUser<DatabaseBase::ListType::WHITELIST>(bot, m);
                } else if (m->text == removefromwhitelist) {
                    handleRemoveUser<DatabaseBase::ListType::WHITELIST>(bot, m);
                } else if (m->text == addtoblacklist) {
                    handleAddUser<DatabaseBase::ListType::BLACKLIST>(bot, m);
                } else if (m->text == removefromblacklist) {
                    handleRemoveUser<DatabaseBase::ListType::BLACKLIST>(bot, m);
                }
                registerer->unregisterCallback(token);
            }
        },
        token);
};

void handleSaveIdCmd(const Bot& bot, const Message::Ptr& message) {
    MessageWrapper wrapper(bot, message);
    if (wrapper.hasExtraText()) {
        std::optional<std::string> fileId;
        std::optional<std::string> fileUniqueId;
        std::vector<std::string> names;
        boost::split(names, wrapper.getExtraText(), isEmptyChar);

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
            ss << "Media with names:" << std::endl;
            for (const auto& name: names) {
                ss << "- " << name << std::endl;
            }
            ss << "added";
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
