#include <StringResManager.hpp>
#include <TgBotWrapper.hpp>
#include <algorithm>
#include <array>
#include <libos/OnTerminateRegistrar.hpp>
#include <libos/libsighandler.hpp>

#include "InstanceClassBase.hpp"

void TgBotWrapper::commandHandler(const command_callback_t& module_callback,
                                  unsigned int authflags, MessagePtr message) {
    static const std::string myName = getBotUser()->username;
    MessageWrapperLimited wrapper(message);

    std::string text = message->text;
    if (wrapper.hasExtraText()) {
        text = text.substr(0, text.size() - wrapper.getExtraText().size());
        boost::trim(text);
    }
    auto v = StringTools::split(text, '@');
    if (v.size() == 2 && v[1] != myName) {
        return;
    }

    if (AuthContext::getInstance()->isAuthorized(message, authflags)) {
        module_callback(shared_from_this(), message);
    }
}

void TgBotWrapper::addCommand(const CommandModule& module, bool isReload) {
    unsigned int authflags = AuthContext::Flags::REQUIRE_USER;
    if (!module.isEnforced()) {
        authflags |= AuthContext::Flags::PERMISSIVE;
    }
    getEvents().onCommand(
        module.command, [this, authflags, module](const Message::Ptr& message) {
            commandHandler(module.fn, authflags, message);
        });
    if (!isReload) {
        _modules.emplace_back(module);
    }
}

void TgBotWrapper::removeCommand(const std::string& cmd) {
    getEvents().onCommand(cmd, {});
}

bool TgBotWrapper::setBotCommands() const {
    std::vector<TgBot::BotCommand::Ptr> buffer;
    for (const auto& cmd : _modules) {
        if (!cmd.isHideDescription()) {
            auto onecommand = std::make_shared<CommandModule>(cmd);
            if (cmd.isEnforced()) {
                onecommand->description += " " + GETSTR_BRACE(OWNER);
            }
            buffer.emplace_back(onecommand);
        }
    }
    try {
        getApi().setMyCommands(buffer);
    } catch (const TgBot::TgException& e) {
        LOG(ERROR) << GETSTR_IS(ERROR_UPDATING_BOT_COMMANDS) << e.what();
        return false;
    }
    return true;
}

std::string TgBotWrapper::getCommandModulesStr() const {
    std::stringstream ss;

    for (const auto& module : _modules) {
        ss << module.command << " ";
    }
    return ss.str();
}

bool TgBotWrapper::unloadCommand(const std::string& command) {
    if (!isKnownCommand(command)) {
        LOG(INFO) << "Command " << command << " is not present";
        return false;
    }
    auto it = findModulePosition(command);
    if (it->isLoaded) {
        removeCommand(command);
        it->isLoaded = false;
        return true;
    }
    LOG(INFO) << "Command " << command << " is already unloaded";
    return false;
}

bool TgBotWrapper::reloadCommand(const std::string& command) {
    if (!isKnownCommand(command)) {
        LOG(INFO) << "Command " << command << " is not present";
        return false;
    }
    auto it = findModulePosition(command);
    if (!it->isLoaded) {
        it->isLoaded = true;
        addCommand(*it, true);
        return true;
    }
    LOG(INFO) << "Command " << command << " is already loaded";
    return false;
}

bool TgBotWrapper::isLoadedCommand(const std::string& command) {
    if (!isKnownCommand(command)) {
        LOG(INFO) << "Command " << command << " is not present";
        return false;
    }
    return findModulePosition(command)->isLoaded;
}

bool TgBotWrapper::isKnownCommand(const std::string& command) {
    return findModulePosition(command) != _modules.end();
}

decltype(TgBotWrapper::_modules)::iterator TgBotWrapper::findModulePosition(
    const std::string& command) {
    return std::ranges::find_if(_modules, [&command](const CommandModule& e) {
        return e.command == command;
    });
}

void TgBotWrapper::startPoll() {
    _bot.getApi().deleteWebhook();

    OnTerminateRegistrar::getInstance()->registerCallback([this]() {
        std::ranges::for_each(_modules, [this](auto& elem) {
            // Clear function pointers, as RTCommandLoader manages it, and
            // dlclose invalidates them
            elem.fn = nullptr;
            unloadCommand(elem.command);
        });
    });

    TgLongPoll longPoll(_bot);
    while (!SignalHandler::isSignaled()) {
        longPoll.start();
    }
}

struct ReplyParamsToMsgTid {
    explicit ReplyParamsToMsgTid(
        const ReplyParametersExt::Ptr& replyParameters) {
        if (replyParameters) {
            tid = replyParameters->messageThreadId;
        } else {
            tid = ReplyParametersExt::kThreadIdNone;
        }
    }
    operator MessageId() const { return tid; }

    MessageThreadId tid;
};

namespace {
void handleTgBotApiEx(TgBot::TgException ex,
                      ReplyParametersExt::Ptr replyParameters) {
    // Allow it if it's FORUM_CLOSED
    if (replyParameters && replyParameters->hasThreadId() &&
        ex.errorCode == TgBot::TgException::ErrorCode::BadRequest &&
        (strstr(ex.what(), "FORUM_CLOSED") != nullptr)) {
        LOG(WARNING) << "Failed to send reply message: Bot attempted to send "
                        "reply message to a closed forum";
    }
    // Goodbye, if it is not
    throw;
}

constexpr bool kDisableNotifications = false;
}  // namespace

#ifdef NDEBUG
#define DEBUG_ASSERT_NONNULL_PARAM(param)
#else
#include <absl/log/check.h>
#define DEBUG_ASSERT_NONNULL_PARAM(param) \
    CHECK((param) != nullptr) << "Parameter " << #param << " is null"
#endif

Message::Ptr TgBotWrapper::sendMessage_impl(
    ChatId chatId, const std::string& text,
    ReplyParametersExt::Ptr replyParameters, GenericReply::Ptr replyMarkup,
    const std::string& parseMode) const {
    try {
        return getApi().sendMessage(
            chatId, text, nullptr, replyParameters, replyMarkup, parseMode,
            kDisableNotifications, {}, ReplyParamsToMsgTid{replyParameters});

    } catch (const TgBot::TgException& ex) {
        handleTgBotApiEx(ex, replyParameters);
        return nullptr;
    }
}

Message::Ptr TgBotWrapper::sendAnimation_impl(
    ChatId chatId, boost::variant<InputFile::Ptr, std::string> animation,
    const std::string& caption, ReplyParametersExt::Ptr replyParameters,
    GenericReply::Ptr replyMarkup, const std::string& parseMode) const {
    try {
        return getApi().sendAnimation(chatId, animation, 0, 0, 0, "", caption,
                                      replyParameters, replyMarkup, parseMode,
                                      kDisableNotifications, {},
                                      ReplyParamsToMsgTid{replyParameters});
    } catch (const TgBot::TgException& ex) {
        handleTgBotApiEx(ex, replyParameters);
        return nullptr;
    }
}

Message::Ptr TgBotWrapper::sendSticker_impl(
    ChatId chatId, boost::variant<InputFile::Ptr, std::string> sticker,
    ReplyParametersExt::Ptr replyParameters) const {
    try {
        return getApi().sendSticker(chatId, sticker, replyParameters, nullptr,
                                    kDisableNotifications,
                                    ReplyParamsToMsgTid{replyParameters});
    } catch (const TgBot::TgException& ex) {
        handleTgBotApiEx(ex, replyParameters);
        return nullptr;
    }
}

Message::Ptr TgBotWrapper::editMessage_impl(
    const Message::Ptr& message, const std::string& newText,
    const TgBot::InlineKeyboardMarkup::Ptr& markup) const {
    DEBUG_ASSERT_NONNULL_PARAM(message);
    return getApi().editMessageText(newText, message->chat->id,
                                    message->messageId, "", "", nullptr,
                                    markup);
}

Message::Ptr TgBotWrapper::editMessageMarkup_impl(
    const StringOrMessage& message, const GenericReply::Ptr& markup) const {
    return std::visit(
        [=, this](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Message::Ptr>) {
                DEBUG_ASSERT_NONNULL_PARAM(arg);
                return getApi().editMessageReplyMarkup(
                    arg->chat->id, arg->messageId, "", markup);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return getApi().editMessageReplyMarkup(0, 0, arg, markup);
            }
            LOG(WARNING) << "No-op editMessageReplyMarkup";
            return Message::Ptr();
        },
        message);
}

MessageId TgBotWrapper::copyMessage_impl(
    ChatId fromChatId, MessageId messageId,
    ReplyParametersExt::Ptr replyParameters) const {
    const auto ret = getApi().copyMessage(
        fromChatId, fromChatId, messageId, "", "", {}, kDisableNotifications,
        replyParameters, std::make_shared<GenericReply>(), false,
        ReplyParamsToMsgTid{replyParameters});
    if (ret) {
        return ret->messageId;
    }
    return 0;
}

void TgBotWrapper::deleteMessage_impl(const Message::Ptr& message) const {
    DEBUG_ASSERT_NONNULL_PARAM(message);
    getApi().deleteMessage(message->chat->id, message->messageId);
}

void TgBotWrapper::deleteMessages_impl(
    ChatId chatId, const std::vector<MessageId>& messageIds) const {
    getApi().deleteMessages(chatId, messageIds);
}

void TgBotWrapper::restrictChatMember_impl(
    ChatId chatId, UserId userId, TgBot::ChatPermissions::Ptr permissions,
    std::uint32_t untilDate) const {
        DEBUG_ASSERT_NONNULL_PARAM(permissions);
    getApi().restrictChatMember(chatId, userId, permissions, untilDate);
}

Message::Ptr TgBotWrapper::sendDocument_impl(
    ChatId chatId, FileOrString document, const std::string& caption,
    ReplyParametersExt::Ptr replyParameters, GenericReply::Ptr replyMarkup,
    const std::string& parseMode) const {

    return getApi().sendDocument(chatId, std::move(document), "", caption,
                                 replyParameters, replyMarkup, parseMode,
                                 kDisableNotifications, {}, false,
                                 ReplyParamsToMsgTid{replyParameters});
}

Message::Ptr TgBotWrapper::sendPhoto_impl(
    ChatId chatId, FileOrString photo, const std::string& caption,
    ReplyParametersExt::Ptr replyParameters, GenericReply::Ptr replyMarkup,
    const std::string& parseMode) const {
    return getApi().sendPhoto(chatId, photo, caption, replyParameters,
                              replyMarkup, parseMode, kDisableNotifications, {},
                              ReplyParamsToMsgTid{replyParameters});
}

Message::Ptr TgBotWrapper::sendVideo_impl(
    ChatId chatId, FileOrString video, const std::string& caption,
    ReplyParametersExt::Ptr replyParameters, GenericReply::Ptr replyMarkup,
    const std::string& parseMode) const {
    return getApi().sendVideo(chatId, video, false, 0, 0, 0, "", caption,
                              replyParameters, replyMarkup, parseMode,
                              kDisableNotifications, {},
                              ReplyParamsToMsgTid{replyParameters});
}

Message::Ptr TgBotWrapper::sendDice_impl(ChatId chatId) const {
    static const std::array<std::string, 6> dices = {"🎲", "🎯", "🏀",
                                                     "⚽", "🎳", "🎰"};

    return getApi().sendDice(
        chatId, kDisableNotifications, nullptr, nullptr,
        dices[Random::getInstance()->generate(dices.size() - 1)]);
}

StickerSet::Ptr TgBotWrapper::getStickerSet_impl(
    const std::string& setName) const {
    return getApi().getStickerSet(setName);
}

bool TgBotWrapper::createNewStickerSet_impl(
    std::int64_t userId, const std::string& name, const std::string& title,
    const std::vector<InputSticker::Ptr>& stickers,
    Sticker::Type stickerType) const {
    return getApi().createNewStickerSet(userId, name, title, stickers,
                                        stickerType);
}

File::Ptr TgBotWrapper::uploadStickerFile_impl(
    std::int64_t userId, InputFile::Ptr sticker,
    const std::string& stickerFormat) const {
        DEBUG_ASSERT_NONNULL_PARAM(sticker);
    return getApi().uploadStickerFile(userId, sticker, stickerFormat);
}

bool TgBotWrapper::downloadFile_impl(const std::filesystem::path& destfilename,
                                     const std::string& fileid) const {
    const auto file = getApi().getFile(fileid);
    if (!file) {
        LOG(INFO) << "File " << fileid << " not found in Telegram servers.";
        return false;
    }
    // Download the file
    std::string buffer = getApi().downloadFile(file->filePath);
    // Save the file to a file on disk
    std::fstream ofs(destfilename, std::ios::binary | std::ios::out);
    if (!ofs.is_open()) {
        LOG(ERROR) << "Failed to open file for writing: " << destfilename;
        return false;
    }
    ofs.write(buffer.data(), buffer.size());
    ofs.close();
    return true;
}

User::Ptr TgBotWrapper::getBotUser_impl() const {
    const static auto me = getApi().getMe();
    return me;
}

bool TgBotWrapper::pinMessage_impl(MessagePtr message) const {
    DEBUG_ASSERT_NONNULL_PARAM(message);
    return getApi().pinChatMessage(message->chat->id, message->messageId, kDisableNotifications);
}

bool TgBotWrapper::unpinMessage_impl(MessagePtr message) const {
    DEBUG_ASSERT_NONNULL_PARAM(message);
    return getApi().unpinChatMessage(message->chat->id, message->messageId);
}

bool TgBotWrapper::banChatMember_impl(const Chat::Ptr& chat,
                                      const User::Ptr& user) const {
                                        DEBUG_ASSERT_NONNULL_PARAM(chat);
    DEBUG_ASSERT_NONNULL_PARAM(user);
    return getApi().banChatMember(chat->id, user->id);
}

bool TgBotWrapper::unbanChatMember_impl(const Chat::Ptr& chat,
                                        const User::Ptr& user) const {
                                            DEBUG_ASSERT_NONNULL_PARAM(chat);
    DEBUG_ASSERT_NONNULL_PARAM(user);
    return getApi().unbanChatMember(chat->id, user->id);
}

User::Ptr TgBotWrapper::getChatMember_impl(ChatId chat, UserId user) const {
    const auto member = getApi().getChatMember(chat, user);
    if (!member) {
        LOG(WARNING) << "ChatMember is null.";
        return {};
    }
    return member->user;
}

DECLARE_CLASS_INST(TgBotWrapper);