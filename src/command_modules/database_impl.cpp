#include <absl/log/check.h>
#include <internal/_tgbot.h>
#include <tgbot/tgbot.h>

#include <StringResManager.hpp>
#include <TgBotWrapper.hpp>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <memory>
#include <optional>

#include "StringToolsExt.hpp"
#include "RandomNumberGenerator.hpp"

template <DatabaseBase::ListType type>
void handleAddUser(TgBotWrapper* wrapper, const Message::Ptr& message,
                   const UserId user) {
    auto base = TgBotDatabaseImpl::getInstance();
    auto res = base->addUserToList(type, user);
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
    wrapper->sendReplyMessage(message, text);
}

template <DatabaseBase::ListType type>
void handleRemoveUser(TgBotWrapper* wrapper, const Message::Ptr& message,
                      const UserId user) {
    auto base = TgBotDatabaseImpl::getInstance();
    auto res = base->removeUserFromList(type, user);
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
    wrapper->sendReplyMessage(message, text);
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

void handleDatabaseCmd(TgBotWrapper* wrapper, const Message::Ptr& message) {
    MessageWrapper msgWrapper(message);

    auto reply = std::make_shared<TgBot::ReplyKeyboardMarkup>();
    reply->keyboard = genKeyboard(4, 2);
    reply->keyboard[0][0]->text = addtowhitelist;
    reply->keyboard[0][1]->text = removefromwhitelist;
    reply->keyboard[1][0]->text = addtoblacklist;
    reply->keyboard[1][1]->text = removefromblacklist;
    reply->oneTimeKeyboard = true;
    reply->resizeKeyboard = true;
    reply->selective = true;

    if (!msgWrapper.switchToReplyToMessage(GETSTR(REPLY_TO_USER_MSG))) {
        return;
    }

    UserId userId = msgWrapper.getUser()->id;

    auto msg =
        wrapper->sendReplyMessage(message,
                                  "Choose what u want to do with " +
                                      UserPtr_toString(msgWrapper.getUser()),
                                  reply);

    const random_return_type token = RandomNumberGenerator::generate(100);

    wrapper->registerCallback(
        [msg, token, userId](TgBotWrapper* wrapper, Message::Ptr m) {
            if (m->replyToMessage &&
                m->replyToMessage->messageId == msg->messageId) {
                if (m->text == addtowhitelist) {
                    handleAddUser<DatabaseBase::ListType::WHITELIST>(wrapper, m,
                                                                     userId);
                } else if (m->text == removefromwhitelist) {
                    handleRemoveUser<DatabaseBase::ListType::WHITELIST>(
                        wrapper, m, userId);
                } else if (m->text == addtoblacklist) {
                    handleAddUser<DatabaseBase::ListType::BLACKLIST>(wrapper, m,
                                                                     userId);
                } else if (m->text == removefromblacklist) {
                    handleRemoveUser<DatabaseBase::ListType::BLACKLIST>(
                        wrapper, m, userId);
                }
                wrapper->unregisterCallback(token);
            }
        },
        token);
};

void handleSaveIdCmd(TgBotWrapper* wrapper, const Message::Ptr& message) {
    MessageWrapper msgWrapper(message);
    if (msgWrapper.hasExtraText()) {
        std::optional<std::string> fileId;
        std::optional<std::string> fileUniqueId;
        std::vector<std::string> names;
        splitWithSpaces(msgWrapper.getExtraText(), names);

        if (!msgWrapper.switchToReplyToMessage(
                GETSTR(REPLY_TO_GIF_OR_STICKER))) {
            return;
        }

        if (msgWrapper.hasAnimation()) {
            fileId = msgWrapper.getAnimation()->fileId;
            fileUniqueId = msgWrapper.getAnimation()->fileUniqueId;
        } else if (msgWrapper.hasSticker()) {
            fileId = msgWrapper.getSticker()->fileId;
            fileUniqueId = msgWrapper.getSticker()->fileUniqueId;
        } else {
            msgWrapper.sendMessageOnExit(GETSTR(REPLY_TO_GIF_OR_STICKER));
            return;
        }

        CHECK(fileId && fileUniqueId) << "They should be set";

        DatabaseBase::MediaInfo info{};
        auto const& backend = TgBotDatabaseImpl::getInstance();
        info.mediaId = fileId.value();
        info.mediaUniqueId = fileUniqueId.value();
        info.names = names;

        msgWrapper.switchToParent();
        if (backend->addMediaInfo(info)) {
            std::stringstream ss;
            ss << "Media with names:" << std::endl;
            for (const auto& name : names) {
                ss << "- " << name << std::endl;
            }
            ss << "added";
            msgWrapper.sendMessageOnExit(ss.str());
        } else {
            msgWrapper.sendMessageOnExit(GETSTR(MEDIA_ALREADY_IN_DB));
        }
    } else {
        msgWrapper.sendMessageOnExit(GETSTR(SEND_A_NAME_TO_SAVE));
    }
}

}  // namespace

DYN_COMMAND_FN(name, module) {
    if (std::string(name) == "database") {
        module.command = "database";
        module.description = GETSTR(DATABASE_CMD_DESC);
        module.flags = CommandModule::Flags::Enforced;
        module.fn = handleDatabaseCmd;
    } else if (std::string(name) == "saveid") {
        module.command = "saveid";
        module.description = GETSTR(SAVEID_CMD_DESC);
        module.flags = CommandModule::Flags::Enforced |
                       CommandModule::Flags::HideDescription;
        module.fn = handleSaveIdCmd;
    } else {
        return false;  // Command not found.
    }
    return true;
}