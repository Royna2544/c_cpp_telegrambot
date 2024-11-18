#include <dlfcn.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <trivial_helpers/_tgbot.h>

#include <Authorization.hpp>
#include <api/TgBotApi.hpp>
#include <api/TgBotApiImpl.hpp>
#include <api/Utils.hpp>
#include <api/components/ChatJoinRequest.hpp>
#include <api/components/ModuleManagement.hpp>
#include <api/components/OnAnyMessage.hpp>
#include <api/components/OnCallbackQuery.hpp>
#include <api/components/OnInlineQuery.hpp>
#include <api/components/OnMyChatMember.hpp>
#include <api/components/Restart.hpp>
#include <api/components/UnknownCommand.hpp>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <libos/libsighandler.hpp>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "ConfigManager.hpp"
#include "StringResLoader.hpp"
#include "api/CommandModule.hpp"
#include "api/MessageExt.hpp"
#include "api/components/FileCheck.hpp"

bool TgBotApiImpl::validateValidArgs(const DynModule* module,
                                     MessageExt::Ptr& message) {
    if (!module->valid_args.enabled) {
        return true;  // No validation needed.
    }
    bool check_argc = module->valid_args.counts != 0;

    // Try to split them.
    const std::vector<std::string>& args =
        message->get<MessageAttrs::ParsedArgumentsList>();

    if (check_argc) {
        std::set<int> valid_argc =
            DynModule::fromArgCountMask(module->valid_args.counts);

        // Check if the number of arguments matches the expected.
        if (!valid_argc.contains(static_cast<int>(args.size()))) {
            std::vector<std::string> strings;
            strings.emplace_back("Invalid number of arguments. Expected");
            if (args.size() < *valid_argc.begin()) {
                strings.emplace_back(
                    fmt::format("at least {}", *valid_argc.begin()));
            } else if (args.size() > *valid_argc.rbegin()) {
                strings.emplace_back(
                    fmt::format("at most {}", *valid_argc.rbegin()));
            } else {
                strings.emplace_back(
                    fmt::format("one of {}", fmt::join(valid_argc, ",")));
            }
            strings.emplace_back(
                fmt::format("arguments. But got {}.", args.size()));

            if (module->valid_args.usage != nullptr) {
                strings.emplace_back(
                    fmt::format("Usage: {}", module->valid_args.usage));
            }
            sendReplyMessage(message->message(),
                             fmt::format("{}", fmt::join(strings, " ")));
            return false;
        }
    }

    return true;
}

bool TgBotApiImpl::isMyCommand(const MessageExt::Ptr& message) const {
    const auto botCommand = message->get<MessageAttrs::BotCommand>();
    const auto target = botCommand.target;
    if (target != getBotUser()->username && !target.empty()) {
        DLOG(INFO) << "Ignore mismatched target: " << std::quoted(target);
        return false;
    }
    return true;
}

bool TgBotApiImpl::authorized(const MessageExt::Ptr& message,
                              const std::string_view commandName,
                              AuthContext::Flags flags) const {
    const auto authRet = _auth->isAuthorized(message->message(), flags);
    if (authRet) {
        return true;
    }
    // Unauthorized user, don't run the command.
    if (message->has<MessageAttrs::User>()) {
        LOG(INFO) << fmt::format("Unauthorized command {} from {}", commandName,
                                 message->get<MessageAttrs::User>());
        switch (authRet.reason) {
            case AuthContext::Result::Reason::UNKNOWN:
                LOG(INFO) << "Reason: Unknown";
                break;
            case AuthContext::Result::Reason::MESSAGE_TOO_OLD:
                LOG(INFO) << "Reason: Message is too old";
                break;
            case AuthContext::Result::Reason::BLACKLISTED_USER:
                LOG(INFO) << "Reason: Blacklisted user";
                break;
            case AuthContext::Result::Reason::GLOBAL_FLAG_OFF:
                LOG(INFO) << "Reason: Global flag is off";
                break;
            case AuthContext::Result::Reason::NOT_IN_WHITELIST:
                LOG(INFO) << "Reason: Not in whitelist";
                break;
            case AuthContext::Result::Reason::REQUIRES_USER:
                LOG(INFO) << "Reason: Requires user";
                break;
            default:
                break;
        }
    }
    return false;
}

void TgBotApiImpl::commandHandler(const std::string& command,
                                  const AuthContext::Flags authflags,
                                  Message::Ptr message) {
    // Find the module first.
    const auto* module = (*kModuleLoader)[command];

    // Create MessageExt object.
    SplitMessageText how = module->_module->valid_args.enabled
                               ? module->_module->valid_args.split_type
                               : SplitMessageText::None;
    auto ext = std::make_shared<MessageExt>(std::move(message), how);

    if (!isMyCommand(ext)) {
        return;
    }

    if (!module->isLoaded()) {
        // Probably unloaded.
        LOG(INFO) << "Command module is unloaded: " << module->_module->name;
        return;
    }

    if (module->_module->function == nullptr) {
        // Just in case.
        LOG(ERROR) << "Command module does not have a function: "
                   << module->_module->name;
        return;
    }

    if (!authorized(ext, command, authflags)) {
        return;
    }

    // Partital offloading to common code.
    if (!validateValidArgs(module->_module, ext)) {
        return;
    }

    const auto msgLocale = ext->get_or<MessageAttrs::Locale>(Locale::Default);
    module->_module->function(this, std::move(ext), (*_loader).at(msgLocale),
                              _provider);
}

bool TgBotApiImpl::unloadCommand(const std::string& command) {
    return (*kModuleLoader) -= command;
}

bool TgBotApiImpl::reloadCommand(const std::string& command) {
    return (*kModuleLoader) += command;
}

void TgBotApiImpl::onAnyMessage(const AnyMessageCallback& callback) {
    onAnyMessageImpl->onAnyMessage(callback);
}

void TgBotApiImpl::onCallbackQuery(
    std::string command,
    TgBot::EventBroadcaster::CallbackQueryListener listener) {
    onCallbackQueryImpl->onCallbackQuery(std::move(command),
                                         std::move(listener));
}

void TgBotApiImpl::addInlineQueryKeyboard(
    InlineQuery query, TgBot::InlineQueryResult::Ptr result) {
    onInlineQueryImpl->add(std::move(query), std::move(result));
}

void TgBotApiImpl::addInlineQueryKeyboard(InlineQuery query,
                                          InlineCallback result) {
    onInlineQueryImpl->add(std::move(query), std::move(result));
}

void TgBotApiImpl::removeInlineQueryKeyboard(const std::string_view key) {
    onInlineQueryImpl->remove(key);
}

void TgBotApiImpl::startPoll() {
    LOG(INFO) << "Bot username: " << getBotUser()->username.value_or("Unknown");
    // Deleting webhook
    getApi().deleteWebhook();

    TgLongPoll longPoll(_bot, 100, 10,
                        {"message", "inline_query", "callback_query",
                         "my_chat_member", "chat_member", "chat_join_request"});
    while (!SignalHandler::isSignaled()) {
        longPoll.start();
    }
}

namespace {
void handleTgBotApiEx(const TgBot::TgException& ex,
                      const ReplyParametersExt::Ptr& replyParameters) {
    // Allow it if it's FORUM_CLOSED
    if (replyParameters && replyParameters->hasThreadId() &&
        ex.errorCode == TgBot::TgException::ErrorCode::BadRequest &&
        (strstr(ex.what(), "FORUM_CLOSED") != nullptr)) {
        LOG(WARNING) << "Failed to send reply message: Bot attempted to send "
                        "reply message to a closed forum";
        return;
    }
    if (ex.errorCode == TgBot::TgException::ErrorCode::BadRequest &&
        (strstr(ex.what(), "message is not modified") != nullptr)) {
        LOG(WARNING) << "Capturing message not modified.";
        return;
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

Message::Ptr TgBotApiImpl::sendMessage_impl(
    ChatId chatId, const std::string_view text,
    ReplyParametersExt::Ptr replyParameters, GenericReply::Ptr replyMarkup,
    const std::string_view parseMode) const {
    try {
        return getApi().sendMessage(chatId, text, globalLinkOptions,
                                    replyParameters, replyMarkup, parseMode,
                                    kDisableNotifications, {},
                                    ReplyParamsToMsgTid{replyParameters});

    } catch (const TgBot::TgException& ex) {
        handleTgBotApiEx(ex, replyParameters);
        return nullptr;
    }
}

Message::Ptr TgBotApiImpl::sendAnimation_impl(
    ChatId chatId, std::variant<InputFile::Ptr, std::string> animation,
    const std::string_view caption, ReplyParametersExt::Ptr replyParameters,
    GenericReply::Ptr replyMarkup, const std::string_view parseMode) const {
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

Message::Ptr TgBotApiImpl::sendSticker_impl(
    ChatId chatId, std::variant<InputFile::Ptr, std::string> sticker,
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

Message::Ptr TgBotApiImpl::editMessage_impl(
    const Message::Ptr& message, const std::string_view newText,
    const TgBot::InlineKeyboardMarkup::Ptr& markup,
    const std::string_view parseMode) const {
    DEBUG_ASSERT_NONNULL_PARAM(message);
    try {
        return getApi().editMessageText(newText, message->chat->id,
                                        message->messageId, "", parseMode,
                                        globalLinkOptions, markup);
    } catch (const TgBot::TgException& ex) {
        handleTgBotApiEx(ex, nullptr);
        return nullptr;
    }
}

Message::Ptr TgBotApiImpl::editMessageMarkup_impl(
    const StringOrMessage& message, const GenericReply::Ptr& markup) const {
    try {
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
    } catch (const TgBot::TgException& ex) {
        handleTgBotApiEx(ex, nullptr);
        return nullptr;
    }
}

MessageId TgBotApiImpl::copyMessage_impl(
    ChatId fromChatId, MessageId messageId,
    ReplyParametersExt::Ptr replyParameters) const {
    const auto ret = getApi().copyMessage(
        fromChatId, fromChatId, messageId, "", "", {}, kDisableNotifications,
        replyParameters, nullptr, false, ReplyParamsToMsgTid{replyParameters});
    if (ret) {
        return ret->messageId;
    }
    return 0;
}

void TgBotApiImpl::deleteMessage_impl(const Message::Ptr& message) const {
    DEBUG_ASSERT_NONNULL_PARAM(message);
    getApi().deleteMessage(message->chat->id, message->messageId);
}

void TgBotApiImpl::deleteMessages_impl(
    ChatId chatId, const std::vector<MessageId>& messageIds) const {
    getApi().deleteMessages(chatId, messageIds);
}

void TgBotApiImpl::restrictChatMember_impl(
    ChatId chatId, UserId userId, TgBot::ChatPermissions::Ptr permissions,
    std::uint32_t untilDate) const {
    DEBUG_ASSERT_NONNULL_PARAM(permissions);
    getApi().restrictChatMember(chatId, userId, permissions, untilDate);
}

Message::Ptr TgBotApiImpl::sendDocument_impl(
    ChatId chatId, FileOrString document, const std::string_view caption,
    ReplyParametersExt::Ptr replyParameters, GenericReply::Ptr replyMarkup,
    const std::string_view parseMode) const {
    return getApi().sendDocument(chatId, std::move(document), "", caption,
                                 replyParameters, replyMarkup, parseMode,
                                 kDisableNotifications, {}, false,
                                 ReplyParamsToMsgTid{replyParameters});
}

Message::Ptr TgBotApiImpl::sendPhoto_impl(
    ChatId chatId, FileOrString photo, const std::string_view caption,
    ReplyParametersExt::Ptr replyParameters, GenericReply::Ptr replyMarkup,
    const std::string_view parseMode) const {
    return getApi().sendPhoto(chatId, photo, caption, replyParameters,
                              replyMarkup, parseMode, kDisableNotifications, {},
                              ReplyParamsToMsgTid{replyParameters});
}

Message::Ptr TgBotApiImpl::sendVideo_impl(
    ChatId chatId, FileOrString video, const std::string_view caption,
    ReplyParametersExt::Ptr replyParameters, GenericReply::Ptr replyMarkup,
    const std::string_view parseMode) const {
    return getApi().sendVideo(chatId, video, false, 0, 0, 0, "", caption,
                              replyParameters, replyMarkup, parseMode,
                              kDisableNotifications, {},
                              ReplyParamsToMsgTid{replyParameters});
}

Message::Ptr TgBotApiImpl::sendDice_impl(ChatId chatId) const {
    static const std::array<std::string, 6> dices = {"🎲", "🎯", "🏀",
                                                     "⚽", "🎳", "🎰"};

    return getApi().sendDice(
        chatId, kDisableNotifications, nullptr, nullptr,
        dices[_provider->random->generate(dices.size() - 1)]);
}

StickerSet::Ptr TgBotApiImpl::getStickerSet_impl(
    const std::string_view setName) const {
    return getApi().getStickerSet(setName);
}

bool TgBotApiImpl::createNewStickerSet_impl(
    std::int64_t userId, const std::string_view name,
    const std::string_view title,
    const std::vector<InputSticker::Ptr>& stickers,
    Sticker::Type stickerType) const {
    return getApi().createNewStickerSet(userId, name, title, stickers,
                                        stickerType);
}

File::Ptr TgBotApiImpl::uploadStickerFile_impl(
    std::int64_t userId, InputFile::Ptr sticker,
    const std::string_view stickerFormat) const {
    DEBUG_ASSERT_NONNULL_PARAM(sticker);
    return getApi().uploadStickerFile(userId, sticker, stickerFormat);
}

bool TgBotApiImpl::downloadFile_impl(const std::filesystem::path& destfilename,
                                     const std::string_view fileid) const {
    const auto file = getApi().getFile(fileid);
    if (!file) {
        LOG(INFO) << "File " << fileid << " not found in Telegram servers.";
        return false;
    }
    if (!file->filePath) {
        LOG(INFO) << "Cannot retrieve filePath";
        return false;
    }
    // Download the file
    std::string buffer = getApi().downloadFile(*file->filePath);
    // Save the file to a file on disk
    std::fstream ofs(destfilename, std::ios::binary | std::ios::out);
    if (!ofs.is_open()) {
        LOG(ERROR) << "Failed to open file for writing: " << destfilename;
        return false;
    }
    ofs.write(buffer.data(), buffer.size());
    ofs.close();
    LOG(INFO) << "Downloaded file " << fileid << " to " << destfilename;
    return true;
}

User::Ptr TgBotApiImpl::getBotUser_impl() const {
    const static auto me = getApi().getMe();
    return me;
}

bool TgBotApiImpl::pinMessage_impl(Message::Ptr message) const {
    DEBUG_ASSERT_NONNULL_PARAM(message);
    return getApi().pinChatMessage(message->chat->id, message->messageId,
                                   kDisableNotifications);
}

bool TgBotApiImpl::unpinMessage_impl(Message::Ptr message) const {
    DEBUG_ASSERT_NONNULL_PARAM(message);
    return getApi().unpinChatMessage(message->chat->id, message->messageId);
}

bool TgBotApiImpl::banChatMember_impl(const Chat::Ptr& chat,
                                      const User::Ptr& user) const {
    DEBUG_ASSERT_NONNULL_PARAM(chat);
    DEBUG_ASSERT_NONNULL_PARAM(user);
    return getApi().banChatMember(chat->id, user->id);
}

bool TgBotApiImpl::unbanChatMember_impl(const Chat::Ptr& chat,
                                        const User::Ptr& user) const {
    DEBUG_ASSERT_NONNULL_PARAM(chat);
    DEBUG_ASSERT_NONNULL_PARAM(user);
    return getApi().unbanChatMember(chat->id, user->id);
}

User::Ptr TgBotApiImpl::getChatMember_impl(ChatId chat, UserId user) const {
    const auto member = getApi().getChatMember(chat, user);
    if (!member) {
        LOG(WARNING) << "ChatMember is null.";
        return {};
    }
    return member->user;
}

void TgBotApiImpl::setDescriptions_impl(
    const std::string_view description,
    const std::string_view shortDescription) const {
    getApi().setMyDescription(description);
    getApi().setMyShortDescription(shortDescription);
}

bool TgBotApiImpl::setMessageReaction_impl(
    const ChatId chatid, const MessageId message,
    const std::vector<ReactionType::Ptr>& reaction, bool isBig) const {
    return getApi().setMessageReaction(chatid, message, reaction, isBig);
}

bool TgBotApiImpl::answerCallbackQuery_impl(
    const std::string_view callbackQueryId, const std::string_view text,
    bool showAlert) const {
    return getApi().answerCallbackQuery(callbackQueryId, text, showAlert);
}

TgBotApiImpl::TgBotApiImpl(const std::string_view token, AuthContext* auth,
                           StringResLoaderBase* loader, Providers* providers)
    : _bot(std::string(token)),
      _auth(auth),
      _loader(loader),
      _provider(providers) {
    globalLinkOptions = std::make_shared<TgBot::LinkPreviewOptions>();
    globalLinkOptions->isDisabled = true;
    // Register -> onUnknownCommand
    onUnknownCommandImpl =
        std::make_unique<TgBotApiImpl::OnUnknownCommandImpl>(this);
    // Register -> onAnyMessage
    onAnyMessageImpl = std::make_unique<TgBotApiImpl::OnAnyMessageImpl>(this);
    // Register->onCallbackQuery
    onCallbackQueryImpl =
        std::make_unique<TgBotApiImpl::OnCallbackQueryImpl>(this);
    // Register -> onInlineQuery
    onInlineQueryImpl =
        std::make_unique<TgBotApiImpl::OnInlineQueryImpl>(_auth, this);
    // Register -> ChatJoinRequest
    onChatJoinRequestImpl =
        std::make_unique<TgBotApiImpl::ChatJoinRequestImpl>(this);
    // Register -> OnMyChatMember
    onMyChatMemberImpl =
        std::make_unique<TgBotApiImpl::OnMyChatMemberImpl>(this);
    // Load modules
    kModuleLoader = std::make_unique<ModulesManagement>(
        this, providers->cmdline->exe().parent_path());
    // Restart command
    restartCommand = std::make_unique<RestartCommand>(this);

    // File Checker using VirusTotal
    if (auto token =
            providers->config->get(ConfigManager::Configs::VIRUSTOTAL_API_KEY);
        token) {
        LOG(INFO) << "Initalizing VirusTotal based file checker";
        virusChecker = std::make_unique<FileCheck>(this, token.value());
    }
}

TgBotApiImpl::~TgBotApiImpl() = default;
