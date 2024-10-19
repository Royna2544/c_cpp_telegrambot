#include <absl/log/check.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <tgbot/types/KeyboardButton.h>
#include <tgbot/types/ReplyKeyboardMarkup.h>
#include <tgbot/types/ReplyKeyboardRemove.h>
#include <trivial_helpers/_tgbot.h>

#include <StringResManager.hpp>
#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <api/TgBotApi.hpp>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <memory>
#include <optional>
#include <string>

using TgBot::ReplyKeyboardRemove;

template <DatabaseBase::ListType type>
void handleAddUser(const InstanceClassBase<TgBotApi>::pointer_type api, const Message::Ptr& message,
                   const UserId user) {
    auto* base = TgBotDatabaseImpl::getInstance();
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
    auto remove = std::make_shared<ReplyKeyboardRemove>();
    remove->removeKeyboard = true;
    api->sendReplyMessage(message, text, remove);
}

template <DatabaseBase::ListType type>
void handleRemoveUser(const InstanceClassBase<TgBotApi>::pointer_type api, const Message::Ptr& message,
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
    auto remove = std::make_shared<ReplyKeyboardRemove>();
    remove->removeKeyboard = true;
    api->sendReplyMessage(message, text, remove);
}

namespace {

using TgBot::KeyboardButton;

template <int X, int Y>
std::vector<std::vector<KeyboardButton::Ptr>> genKeyboard() {
    std::vector<std::vector<KeyboardButton::Ptr>> keyboard;
    keyboard.reserve(Y);
    for (int i = 0; i < Y; i++) {
        std::vector<KeyboardButton::Ptr> row;
        row.reserve(X);
        for (int j = 0; j < X; j++) {
            row.emplace_back(std::make_shared<KeyboardButton>());
        }
        keyboard.emplace_back(std::move(row));
    }
    return keyboard;
}

constexpr std::string_view addtowhitelist = "Add to whitelist";
constexpr std::string_view removefromwhitelist = "Remove from whitelist";
constexpr std::string_view addtoblacklist = "Add to blacklist";
constexpr std::string_view removefromblacklist = "Remove from blacklist";

DECLARE_COMMAND_HANDLER(database, wrapper, message) {
    if (!message->replyMessage()->exists()) {
        wrapper->sendReplyMessage(message->message(),
                                  GETSTR(REPLY_TO_USER_MSG));
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

    UserId userId = message->replyMessage()->get<MessageAttrs::User>()->id;

    auto msg = wrapper->sendReplyMessage(
        message->message(),
        fmt::format("Choose what u want to do with {}",
                    message->replyMessage()->get<MessageAttrs::User>()),
        reply);

    wrapper->onAnyMessage(
        [msg, userId](const InstanceClassBase<TgBotApi>::pointer_type api, const Message::Ptr& m) {
            if (m->replyToMessage &&
                m->replyToMessage->messageId == msg->messageId) {
                if (m->text == addtowhitelist) {
                    handleAddUser<DatabaseBase::ListType::WHITELIST>(api, m,
                                                                     userId);
                } else if (m->text == removefromwhitelist) {
                    handleRemoveUser<DatabaseBase::ListType::WHITELIST>(api, m,
                                                                        userId);
                } else if (m->text == addtoblacklist) {
                    handleAddUser<DatabaseBase::ListType::BLACKLIST>(api, m,
                                                                     userId);
                } else if (m->text == removefromblacklist) {
                    handleRemoveUser<DatabaseBase::ListType::BLACKLIST>(api, m,
                                                                        userId);
                }
                return TgBotApi::AnyMessageResult::Deregister;
            }
            return TgBotApi::AnyMessageResult::Handled;
        });
};

DECLARE_COMMAND_HANDLER(saveid, bot, message) {
    if (!(message->has<MessageAttrs::ExtraText>() &&
          message->replyMessage()->any(
              {MessageAttrs::Animation, MessageAttrs::Sticker}))) {
        bot->sendReplyMessage(message->message(),
                              GETSTR(REPLY_TO_GIF_OR_STICKER));
        return;
    }
    std::optional<std::string> fileId;
    std::optional<std::string> fileUniqueId;
    DatabaseBase::MediaType type{};

    if (message->replyMessage()->has<MessageAttrs::Animation>()) {
        const auto p = message->replyMessage()->get<MessageAttrs::Animation>();
        fileId = p->fileId;
        fileUniqueId = p->fileUniqueId;
        type = DatabaseBase::MediaType::GIF;
    } else if (message->replyMessage()->has<MessageAttrs::Sticker>()) {
        const auto p = message->replyMessage()->get<MessageAttrs::Sticker>();
        fileId = p->fileId;
        fileUniqueId = p->fileUniqueId;
        type = DatabaseBase::MediaType::STICKER;
    }

    CHECK(fileId && fileUniqueId) << "They should be set";

    DatabaseBase::MediaInfo info{};
    auto const& backend = TgBotDatabaseImpl::getInstance();
    info.mediaId = fileId.value();
    info.mediaUniqueId = fileUniqueId.value();
    info.names = message->get<MessageAttrs::ParsedArgumentsList>();
    info.mediaType = type;

    if (backend->addMediaInfo(info)) {
        const auto content = fmt::format("Media with names:\n{}\nadded",
                                         fmt::join(info.names, "\n"));
        bot->sendReplyMessage(message->message(), content);
    } else {
        bot->sendReplyMessage(message->message(), GETSTR(MEDIA_ALREADY_IN_DB));
    }
}

}  // namespace

DYN_COMMAND_FN(name, module) {
    if (name == "database") {
        module.name = name;
        module.description = GETSTR(DATABASE_CMD_DESC);
        module.flags = CommandModule::Flags::Enforced;
        module.function = COMMAND_HANDLER_NAME(database);
    } else if (name == "saveid") {
        module.name = name;
        module.description = GETSTR(SAVEID_CMD_DESC);
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