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
#include <api/StringResLoader.hpp>
#include <api/TgBotApi.hpp>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <memory>
#include <optional>
#include <string>

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

DECLARE_COMMAND_HANDLER(database) {
    if (!message->reply()->exists()) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::REPLY_TO_USER_MSG));
        return;
    }

    auto reply = std::make_shared<TgBot::ReplyKeyboardMarkup>();
    reply->keyboard = genKeyboard<2, 2>();
    const std::string addToWhitelist = std::string(res->get(Strings::DB_ADD_TO_WHITELIST));
    const std::string removeFromWhitelist =
        std::string(res->get(Strings::DB_REMOVE_FROM_WHITELIST));
    const std::string addToBlacklist = std::string(res->get(Strings::DB_ADD_TO_BLACKLIST));
    const std::string removeFromBlacklist =
        std::string(res->get(Strings::DB_REMOVE_FROM_BLACKLIST));
    reply->keyboard.at(0).at(0)->text = addToWhitelist;
    reply->keyboard.at(0).at(1)->text = removeFromWhitelist;
    reply->keyboard.at(1).at(0)->text = addToBlacklist;
    reply->keyboard.at(1).at(1)->text = removeFromBlacklist;
    reply->oneTimeKeyboard = true;
    reply->resizeKeyboard = true;
    reply->selective = true;

    UserId userId = message->reply()->get<MessageAttrs::User>()->id;

    // The admin who invoked /database; only they may drive the resulting
    // keyboard, otherwise any user replying to the prompt could mutate the
    // black/whitelist (the command is Enforced, but onAnyMessage fires for
    // everyone).
    const auto invoker = message->message()->from;
    const UserId invokerId = invoker ? (*invoker)->id : 0;

    auto msg = api->sendReplyMessage(
        message->message(),
        fmt::format(fmt::runtime(res->get(Strings::DB_CHOOSE_USER_ACTION)),
                    message->reply()->get<MessageAttrs::User>()),
        reply);

    api->onAnyMessage([msg, userId, invokerId, res, provider, addToWhitelist,
                       removeFromWhitelist, addToBlacklist,
                       removeFromBlacklist](TgBotApi::CPtr api,
                                            const Message::Ptr& m) {
        if (invokerId != 0 && m->from && (*m->from)->id == invokerId &&
            m->replyToMessage &&
            (*m->replyToMessage)->messageId == msg->messageId) {
            Strings text{};
            if (m->text == addToWhitelist) {
                text = handleAddUser<DatabaseBase::ListType::WHITELIST>(
                    provider, userId);
            } else if (m->text == removeFromWhitelist) {
                text = handleRemoveUser<DatabaseBase::ListType::WHITELIST>(
                    provider, userId);
            } else if (m->text == addToBlacklist) {
                text = handleAddUser<DatabaseBase::ListType::BLACKLIST>(
                    provider, userId);
            } else if (m->text == removeFromBlacklist) {
                text = handleRemoveUser<DatabaseBase::ListType::BLACKLIST>(
                    provider, userId);
            }
            auto remove = std::make_shared<ReplyKeyboardRemove>();
            remove->removeKeyboard = true;
            api->sendReplyMessage(m, res->get(text), remove);
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
                              res->get(Strings::REPLY_TO_GIF_OR_STICKER));
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

    switch (provider->database->addMediaInfo(info)) {
        case DatabaseBase::AddResult::OK: {
            const auto content = fmt::format(fmt::runtime(res->get(Strings::DB_MEDIA_ADDED)),
                                             fmt::join(info.names, "\n"));
            api->sendReplyMessage(message->message(), content);
            break;
        }
        case DatabaseBase::AddResult::ALREADY_EXISTS:

            api->sendReplyMessage(message->message(),
                                  res->get(Strings::MEDIA_ALREADY_IN_DB));
            break;
        case DatabaseBase::AddResult::BACKEND_ERROR:
            api->sendReplyMessage(message->message(),
                                  res->get(Strings::BACKEND_ERROR));
            break;
    }
}

}  // namespace

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
#ifdef cmd_database_EXPORTS
    .flags = DynModule::Flags::Enforced,
    .name = "database",
    .description = "Run database commands",
    .function = COMMAND_HANDLER_NAME(database),
    .valid_args = {},
#endif
#ifdef cmd_saveid_EXPORTS
    .flags = DynModule::Flags::Enforced | DynModule::Flags::HideDescription,
    .name = "saveid",
    .description = "Save a Telegram media as name",
    .function = COMMAND_HANDLER_NAME(saveid),
    .valid_args =
        {
            .enabled = true,
            .split_type = DynModule::ValidArgs::Split::ByComma,
            .usage = "/saveid <names>,...",
        },
#endif
};
