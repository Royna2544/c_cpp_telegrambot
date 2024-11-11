#include <absl/log/check.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <tgbot/types/KeyboardButton.h>
#include <tgbot/types/ReplyKeyboardMarkup.h>
#include <tgbot/types/ReplyKeyboardRemove.h>
#include <trivial_helpers/_tgbot.h>

#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <api/Providers.hpp>
#include <api/TgBotApi.hpp>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <memory>
#include <optional>
#include <string>

#include "StringResLoader.hpp"

using TgBot::ReplyKeyboardRemove;

template <DatabaseBase::ListType type>
Strings handleAddUser(const Providers* provider, const UserId user) {
    auto res = provider->database.get()->addUserToList(type, user);
    Strings text{};
    switch (res) {
        case DatabaseBase::ListResult::OK:
            text = Strings::USER_ADDED;
            break;
        case DatabaseBase::ListResult::ALREADY_IN_LIST:
            text = Strings::USER_ALREADY_IN_LIST;
            break;
        case DatabaseBase::ListResult::ALREADY_IN_OTHER_LIST:
            text = Strings::USER_ALREADY_IN_OTHER_LIST;
            break;
        case DatabaseBase::ListResult::BACKEND_ERROR:
            text = Strings::BACKEND_ERROR;
            break;
        default:
            LOG(ERROR) << "Unhandled result type " << static_cast<int>(res);
            text = Strings::UNKNOWN_ERROR;
    }
    return text;
}

template <DatabaseBase::ListType type>
Strings handleRemoveUser(const Providers* provider, const UserId user) {
    auto res = provider->database->removeUserFromList(type, user);
    Strings text{};
    switch (res) {
        case DatabaseBase::ListResult::OK:
            text = Strings::USER_REMOVED;
            break;
        case DatabaseBase::ListResult::NOT_IN_LIST:
            text = Strings::USER_NOT_IN_LIST;
            break;
        case DatabaseBase::ListResult::ALREADY_IN_OTHER_LIST:
            text = Strings::USER_ALREADY_IN_OTHER_LIST;
            break;
        case DatabaseBase::ListResult::BACKEND_ERROR:
            text = Strings::BACKEND_ERROR;
            break;
        default:
            LOG(ERROR) << "Unhandled result type " << static_cast<int>(res);
            text = Strings::UNKNOWN_ERROR;
    }
    return text;
}

namespace {

using TgBot::KeyboardButton;

template <int X, int Y>
std::vector<std::vector<KeyboardButton::Ptr>> genKeyboard() {
    std::vector<std::vector<KeyboardButton::Ptr>> keyboard;
    keyboard.resize(Y);
    for (int i = 0; i < Y; i++) {
        std::vector<KeyboardButton::Ptr> row;
        row.resize(X);
        for (int j = 0; j < X; j++) {
            row[j] = std::make_shared<KeyboardButton>();
        }
        keyboard[i] = std::move(row);
    }
    return keyboard;
}

constexpr std::string_view addtowhitelist = "Add to whitelist";
constexpr std::string_view removefromwhitelist = "Remove from whitelist";
constexpr std::string_view addtoblacklist = "Add to blacklist";
constexpr std::string_view removefromblacklist = "Remove from blacklist";

DECLARE_COMMAND_HANDLER(database) {
    if (!message->reply()->exists()) {
        api->sendReplyMessage(message->message(),
                                  access(res, Strings::REPLY_TO_USER_MSG));
        return;
    }

    auto reply = std::make_shared<TgBot::ReplyKeyboardMarkup>();
    reply->keyboard = genKeyboard<2, 2>();
    reply->keyboard.at(0).at(0)->text = addtowhitelist;
    reply->keyboard.at(0).at(1)->text = removefromwhitelist;
    reply->keyboard.at(1).at(0)->text = addtoblacklist;
    reply->keyboard.at(1).at(1)->text = removefromblacklist;
    reply->oneTimeKeyboard = true;
    reply->resizeKeyboard = true;
    reply->selective = true;

    UserId userId = message->reply()->get<MessageAttrs::User>()->id;

    auto msg = api->sendReplyMessage(
        message->message(),
        fmt::format("Choose what u want to do with {}",
                    message->reply()->get<MessageAttrs::User>()),
        reply);

    api->onAnyMessage(
        [msg, userId, res, provider](TgBotApi::CPtr api, const Message::Ptr& m) {
            if (m->replyToMessage &&
                m->replyToMessage->messageId == msg->messageId) {
                Strings text{};
                if (m->text == addtowhitelist) {
                    text = handleAddUser<DatabaseBase::ListType::WHITELIST>(
                        provider, userId);
                } else if (m->text == removefromwhitelist) {
                    text = handleRemoveUser<DatabaseBase::ListType::WHITELIST>(
                        provider, userId);
                } else if (m->text == addtoblacklist) {
                    text = handleAddUser<DatabaseBase::ListType::BLACKLIST>(
                        provider, userId);
                } else if (m->text == removefromblacklist) {
                    text = handleRemoveUser<DatabaseBase::ListType::BLACKLIST>(
                        provider, userId);
                }
                auto remove = std::make_shared<ReplyKeyboardRemove>();
                remove->removeKeyboard = true;
                api->sendReplyMessage(m, access(res, text), remove);
                return TgBotApi::AnyMessageResult::Deregister;
            }
            return TgBotApi::AnyMessageResult::Handled;
        });
};

DECLARE_COMMAND_HANDLER(saveid) {
    if (!(message->has<MessageAttrs::ExtraText>() &&
          message->reply()->any(
              {MessageAttrs::Animation, MessageAttrs::Sticker}))) {
        api->sendReplyMessage(message->message(),
                              access(res, Strings::REPLY_TO_GIF_OR_STICKER));
        return;
    }
    std::optional<std::string> fileId;
    std::optional<std::string> fileUniqueId;
    DatabaseBase::MediaType type{};

    if (message->reply()->has<MessageAttrs::Animation>()) {
        const auto p = message->reply()->get<MessageAttrs::Animation>();
        fileId = p->fileId;
        fileUniqueId = p->fileUniqueId;
        type = DatabaseBase::MediaType::GIF;
    } else if (message->reply()->has<MessageAttrs::Sticker>()) {
        const auto p = message->reply()->get<MessageAttrs::Sticker>();
        fileId = p->fileId;
        fileUniqueId = p->fileUniqueId;
        type = DatabaseBase::MediaType::STICKER;
    }

    CHECK(fileId && fileUniqueId) << "They should be set";

    DatabaseBase::MediaInfo info{};
    info.mediaId = fileId.value();
    info.mediaUniqueId = fileUniqueId.value();
    info.names = message->get<MessageAttrs::ParsedArgumentsList>();
    info.mediaType = type;

    if (provider->database->addMediaInfo(info)) {
        const auto content = fmt::format("Media with names:\n{}\nadded",
                                         fmt::join(info.names, "\n"));
        api->sendReplyMessage(message->message(), content);
    } else {
        api->sendReplyMessage(message->message(),
                              access(res, Strings::MEDIA_ALREADY_IN_DB));
    }
}

}  // namespace

DYN_COMMAND_FN(name, module) {
    if (name == "database") {
        module.name = name;
        module.description = "Run database commands";
        module.flags = CommandModule::Flags::Enforced;
        module.function = COMMAND_HANDLER_NAME(database);
    } else if (name == "saveid") {
        module.name = name;
        module.description = "Save a Telegram media as name";
        module.flags = CommandModule::Flags::Enforced |
                       CommandModule::Flags::HideDescription;
        module.function = COMMAND_HANDLER_NAME(saveid);
        module.valid_arguments.enabled = true;
        module.valid_arguments.split_type =
            CommandModule::ValidArgs::Split::ByComma;
    } else {
        return false;  // Command not found.
    }
    return true;
}