#include <absl/log/log.h>
#include <absl/strings/match.h>
#include <dlfcn.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <trivial_helpers/_tgbot.h>

#include <CommandLine.hpp>
#include <ConfigManager.hpp>
#include <DurationPoint.hpp>
#include <GitBuildInfo.hpp>
#include <api/AuthContext.hpp>
#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
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
#include <chrono>
#include <cpptrace/cpptrace.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <libos/libsighandler.hpp>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "tgbot/net/CurlHttpClient.h"

template <>
struct fmt::formatter<CommandModule::Info::Type> : formatter<std::string_view> {
    // parse is inherited from formatter<string_view>.
    auto format(CommandModule::Info::Type c, format_context& ctx) const
        -> format_context::iterator {
        string_view name = "unknown";
        switch (c) {
            case CommandModule::Info::Type::SharedLib:
                name = "SharedLib";
                break;
            case CommandModule::Info::Type::Lua:
                name = "Lua";
                break;
            default:
                break;
        }
        return formatter<string_view>::format(name, ctx);
    }
};

bool TgBotApiImpl::validateValidArgs(const CommandModule::Info* module,
                                     MessageExt::Ptr message) {
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

        const int in_args_size = static_cast<int>(args.size());
        // Check if the number of arguments matches the expected.
        if (!valid_argc.contains(in_args_size)) {
            std::vector<std::string> strings;
            strings.emplace_back("Invalid number of arguments. Expected");
            if (in_args_size < *valid_argc.begin()) {
                strings.emplace_back(
                    fmt::format("at least {}", *valid_argc.begin()));
            } else if (in_args_size > *valid_argc.rbegin()) {
                strings.emplace_back(
                    fmt::format("at most {}", *valid_argc.rbegin()));
            } else {
                strings.emplace_back(
                    fmt::format("one of {}", fmt::join(valid_argc, ",")));
            }
            strings.emplace_back(
                fmt::format("arguments. But got {}.", args.size()));

            if (!module->valid_args.usage.empty()) {
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
                              AuthContext::AccessLevel flags) const {
    const auto authRet = _auth->isAuthorized(message->message(), flags);
    if (authRet) {
        return true;
    }
    // Unauthorized user, don't run the command.
    if (message->has<MessageAttrs::User>()) {
        LOG(INFO) << fmt::format("Unauthorized command {} from {}", commandName,
                                 message->get<MessageAttrs::User>());
        switch (authRet.result.second) {
            case AuthContext::Result::Reason::Unknown:
                LOG(INFO) << "Reason: Unknown";
                break;
            case AuthContext::Result::Reason::MessageTooOld:
                LOG(INFO) << "Reason: Message is too old";
                break;
            case AuthContext::Result::Reason::ForbiddenUser:
                LOG(INFO) << "Reason: Forbidden user (Blacklisted)";
                break;
            case AuthContext::Result::Reason::PermissionDenied:
                LOG(INFO) << "Reason: Permission denied (Not in whitelist)";
                break;
            case AuthContext::Result::Reason::UserIsBot:
                LOG(INFO) << "Reason: Is a bot";
                break;
            default:
                break;
        }
    }
    return false;
}

void TgBotApiImpl::commandHandler(const std::string& command,
                                  const AuthContext::AccessLevel authflags,
                                  Message::Ptr message) {
    // Find the module first.
    const auto* module = (*kModuleLoader)[command];

    // Create MessageExt object.
    SplitMessageText how = module->info.valid_args.enabled
                               ? module->info.valid_args.split_type
                               : SplitMessageText::None;
    auto ext = std::make_unique<MessageExt>(std::move(message), how);

    if (!isMyCommand(ext.get())) {
        return;
    }

    if (!module->isLoaded()) {
        // Probably unloaded.
        LOG(INFO) << "Command module is unloaded: " << module->info.name;
        return;
    }

    if (!authorized(ext.get(), command, authflags)) {
        return;
    }

    if (!_rateLimiter.check()) {
        LOG(INFO) << fmt::format("Ratelimiting user {}",
                                 ext->get<MessageAttrs::User>());
        return;
    }

    // Partital offloading to common code.
    if (!validateValidArgs(&module->info, ext.get())) {
        return;
    }

    [[maybe_unused]] MilliSecondDP dp;
    module->info.function(this, ext.get(),
                          _loader->at(ext->get<MessageAttrs::Locale>()),
                          _provider);
    if constexpr (buildinfo::isDebugBuild()) {
        DLOG(INFO) << fmt::format("Executing cmd {} took {} ({})", command,
                                  dp.get(), module->info.module_type);
    }
}

void TgBotApiImpl::addCommandListener(CommandListener* listener) {
    _listeners.emplace_back(listener);
}

bool TgBotApiImpl::unloadCommand(const std::string& command) {
    DLOG(INFO) << "Notifying onUnload listeners";
    for (auto* listener : _listeners) {
        listener->onUnload(command);
    }
    DLOG(INFO) << "Done notifying";
    // Remove the command from the loader.
    return kModuleLoader->unload(command);
}

bool TgBotApiImpl::reloadCommand(const std::string& command) {
    DLOG(INFO) << "Notifying onReload listeners";
    for (auto* listener : _listeners) {
        listener->onReload(command);
    }
    DLOG(INFO) << "Done notifying";
    // Reload the command to the loader.
    return kModuleLoader->load(command);
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
    LOG(INFO) << "Bot username: " << getBotUser()->username.value();
    // Deleting webhook
    getApi().deleteWebhook();

    getApi().setMyDescription(fmt::format(
        "A C++ written Telegram bot, sources: {}", buildinfo::git::ORIGIN_URL));

    std::string ownerString;
    if (auto owner = _provider->database->getOwnerUserId(); owner) {
        auto chat = getApi().getChat(*owner);
        if (chat->username)
            ownerString = fmt::format(" Owned by @{}.", *chat->username);
    }

    getApi().setMyShortDescription(
        fmt::format("C++ Telegram bot.{} I'm currently hosted on {}",
                    ownerString, buildinfo::OS));

    auto* longPoll = _bot.createLongPoll(
        {}, {},
        TgBot::Update::Types::message | TgBot::Update::Types::inline_query |
            TgBot::Update::Types::callback_query |
            TgBot::Update::Types::my_chat_member |
            TgBot::Update::Types::chat_member |
            TgBot::Update::Types::chat_join_request);

    // Start the long poll loop.
    while (!SignalHandler::isSignaled()) {
        longPoll->start();
    }
}

namespace {
void handleTgBotApiEx(const TgBot::TgException& ex) {
    switch (ex.errorCode) {
        case TgBot::TgException::ErrorCode::BadRequest: {
            if (absl::StrContains(ex.what(), "FORUM_CLOSED")) {
                LOG(WARNING) << "Forum closed. Skipping message.";
                return;
            }
            if (absl::StrContains(ex.what(), "message is not modified")) {
                LOG(WARNING) << "Capturing message not modified.";
                return;
            }
            // Intentional fallthrough for other BadRequest errors
            [[fallthrough]];
        }
        default:
            break;
    }
    LOG(ERROR) << "TgBotAPI exception: " << ex.what();
    cpptrace::generate_trace().print();
    throw;
}

constexpr bool kDisableNotifications = false;
}  // namespace

Message::Ptr TgBotApiImpl::sendMessage_impl(
    ChatId chatId, const std::string_view text,
    ReplyParametersExt::Ptr replyParameters, GenericReply::Ptr replyMarkup,
    const TgBot::Api::ParseMode parseMode) const {
    try {
        return getApi().sendMessage(chatId, text, globalLinkOptions,
                                    replyParameters, std::move(replyMarkup),
                                    parseMode, kDisableNotifications, {},
                                    ReplyParamsToMsgTid{replyParameters});

    } catch (const TgBot::TgException& ex) {
        handleTgBotApiEx(ex);
        return nullptr;
    }
}

Message::Ptr TgBotApiImpl::sendAnimation_impl(
    ChatId chatId, std::variant<InputFile::Ptr, std::string> animation,
    const std::string_view caption, ReplyParametersExt::Ptr replyParameters,
    GenericReply::Ptr replyMarkup, const ParseMode parseMode) const {
    try {
        return getApi().sendAnimation(chatId, animation, {}, {}, {}, {},
                                      caption, replyParameters, replyMarkup,
                                      parseMode, kDisableNotifications, {},
                                      ReplyParamsToMsgTid{replyParameters});
    } catch (const TgBot::TgException& ex) {
        handleTgBotApiEx(ex);
        return nullptr;
    }
}

Message::Ptr TgBotApiImpl::sendSticker_impl(
    ChatId chatId, std::variant<InputFile::Ptr, std::string> sticker,
    ReplyParametersExt::Ptr replyParameters) const {
    getApi().sendChatAction(chatId, TgBot::Api::ChatAction::choose_sticker,
                            ReplyParamsToMsgTid{replyParameters});
    try {
        return getApi().sendSticker(chatId, sticker, replyParameters, nullptr,
                                    kDisableNotifications,
                                    ReplyParamsToMsgTid{replyParameters});
    } catch (const TgBot::TgException& ex) {
        handleTgBotApiEx(ex);
        return nullptr;
    }
}

Message::Ptr TgBotApiImpl::editMessage_impl(
    const Message::Ptr& message, const std::string_view newText,
    const TgBot::InlineKeyboardMarkup::Ptr& markup,
    const ParseMode parseMode) const {
    DCHECK_NE(message, nullptr);
    try {
        return getApi().editMessageText(newText, message->chat->id,
                                        message->messageId, {}, parseMode,
                                        globalLinkOptions, markup);
    } catch (const TgBot::TgException& ex) {
        handleTgBotApiEx(ex);
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
                    DCHECK_NE(arg, nullptr);
                    return getApi().editMessageReplyMarkup(
                        arg->chat->id, arg->messageId, {}, markup);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return getApi().editMessageReplyMarkup({}, {}, arg, markup);
                }
                LOG(WARNING) << "No-op editMessageReplyMarkup";
                return Message::Ptr();
            },
            message);
    } catch (const TgBot::TgException& ex) {
        handleTgBotApiEx(ex);
        return nullptr;
    }
}

MessageId TgBotApiImpl::copyMessage_impl(
    ChatId fromChatId, MessageId messageId,
    ReplyParametersExt::Ptr replyParameters) const {
    const auto ret = getApi().copyMessage(
        fromChatId, fromChatId, messageId, {}, {}, {}, kDisableNotifications,
        replyParameters, nullptr, {}, ReplyParamsToMsgTid{replyParameters});
    if (ret) {
        return ret->messageId;
    }
    return 0;
}

void TgBotApiImpl::deleteMessage_impl(const Message::Ptr& message) const {
    DCHECK_NE(message, nullptr);
    getApi().deleteMessage(message->chat->id, message->messageId);
}

void TgBotApiImpl::deleteMessages_impl(
    ChatId chatId, const std::vector<MessageId>& messageIds) const {
    getApi().deleteMessages(chatId, messageIds);
}

void TgBotApiImpl::restrictChatMember_impl(
    ChatId chatId, UserId userId, TgBot::ChatPermissions::Ptr permissions,
    std::chrono::system_clock::time_point untilDate) const {
    DCHECK_NE(permissions, nullptr);
    getApi().restrictChatMember(chatId, userId, permissions, untilDate);
}

Message::Ptr TgBotApiImpl::sendDocument_impl(
    ChatId chatId, FileOrString document, const std::string_view caption,
    ReplyParametersExt::Ptr replyParameters, GenericReply::Ptr replyMarkup,
    const ParseMode parseMode) const {
    getApi().sendChatAction(chatId, TgBot::Api::ChatAction::upload_document,
                            ReplyParamsToMsgTid{replyParameters});
    return getApi().sendDocument(chatId, std::move(document), {}, caption,
                                 replyParameters, replyMarkup, parseMode,
                                 kDisableNotifications, {}, {},
                                 ReplyParamsToMsgTid{replyParameters});
}

Message::Ptr TgBotApiImpl::sendPhoto_impl(
    ChatId chatId, FileOrString photo, const std::string_view caption,
    ReplyParametersExt::Ptr replyParameters, GenericReply::Ptr replyMarkup,
    const ParseMode parseMode) const {
    getApi().sendChatAction(chatId, TgBot::Api::ChatAction::upload_photo,
                            ReplyParamsToMsgTid{replyParameters});
    return getApi().sendPhoto(chatId, photo, caption, replyParameters,
                              replyMarkup, parseMode, kDisableNotifications, {},
                              ReplyParamsToMsgTid{replyParameters});
}

Message::Ptr TgBotApiImpl::sendVideo_impl(
    ChatId chatId, FileOrString video, const std::string_view caption,
    ReplyParametersExt::Ptr replyParameters, GenericReply::Ptr replyMarkup,
    const ParseMode parseMode) const {
    getApi().sendChatAction(chatId, TgBot::Api::ChatAction::upload_video,
                            ReplyParamsToMsgTid{replyParameters});
    return getApi().sendVideo(chatId, video, {}, {}, {}, {}, {}, caption,
                              replyParameters, replyMarkup, parseMode,
                              kDisableNotifications, {},
                              ReplyParamsToMsgTid{replyParameters});
}

Message::Ptr TgBotApiImpl::sendDice_impl(ChatId chatId) const {
    static constexpr std::array<std::string_view, 6> dices = {"🎲", "🎯", "🏀",
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
    const TgBot::Api::StickerFormat stickerFormat) const {
    DCHECK_NE(sticker, nullptr);
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
    const static bool s = [this] {
        me = getApi().getMe();
        return true;
    }();
    return me;
}

bool TgBotApiImpl::pinMessage_impl(Message::Ptr message) const {
    DCHECK_NE(message, nullptr);
    return getApi().pinChatMessage(message->chat->id, message->messageId,
                                   kDisableNotifications);
}

bool TgBotApiImpl::unpinMessage_impl(Message::Ptr message) const {
    DCHECK_NE(message, nullptr);
    return getApi().unpinChatMessage(message->chat->id, message->messageId);
}

bool TgBotApiImpl::banChatMember_impl(const Chat::Ptr& chat,
                                      const User::Ptr& user) const {
    DCHECK_NE(chat, nullptr);
    DCHECK_NE(user, nullptr);
    return getApi().banChatMember(chat->id, user->id);
}

bool TgBotApiImpl::unbanChatMember_impl(const Chat::Ptr& chat,
                                        const User::Ptr& user) const {
    DCHECK_NE(chat, nullptr);
    DCHECK_NE(user, nullptr);
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
                           StringResLoader* loader, Providers* providers)
    : _bot(std::string(token),
           std::make_unique<TgBot::CurlHttpClient>(std::chrono::seconds(30))),
      _auth(auth),
      _loader(loader),
      _provider(providers),
      _rateLimiter(2, std::chrono::seconds(3)) {
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
    // Load modules (../lib/modules)
    kModuleLoader = std::make_unique<ModulesManagement>(
        this, providers->cmdline->getPath(FS::PathType::CMD_MODULES));
    // Restart command
    restartCommand = std::make_unique<RestartCommand>(this);
}

TgBotApiImpl::~TgBotApiImpl() = default;
