#include <absl/strings/ascii.h>
#include <absl/strings/match.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>
#include <dlfcn.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <trivial_helpers/_tgbot.h>

#include <Authorization.hpp>
#include <Random.hpp>
#include <algorithm>
#include <api/TgBotApiImpl.hpp>
#include <array>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <libos/libsighandler.hpp>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <utility>

#include "StringResLoader.hpp"
#include "api/Utils.hpp"
#include "tgbot/types/CallbackQuery.h"
#include "tgbot/types/ChatJoinRequest.h"
#include "tgbot/types/InlineKeyboardButton.h"
#include "tgbot/types/InlineKeyboardMarkup.h"

bool TgBotApiImpl::validateValidArgs(const CommandModule::Ptr& module,
                                     MessageExt::Ptr& message) {
    if (!module->valid_arguments.enabled) {
        return true;  // No validation needed.
    }
    bool check_argc = !module->valid_arguments.counts.empty();

    // Try to split them.
    const std::vector<std::string>& args =
        message->get<MessageAttrs::ParsedArgumentsList>();
    if (check_argc) {
        std::vector<int> _valid_argc = module->valid_arguments.counts;
        // Sort the valid_arguments
        std::ranges::sort(_valid_argc);
        // Push it to a set, so remove duplicates.
        std::set valid_argc(_valid_argc.begin(), _valid_argc.end());

        // Check if the number of arguments matches the expected.
        if (!valid_argc.contains(static_cast<int>(args.size()))) {
            std::vector<std::string> strings;
            strings.emplace_back("Invalid number of arguments. Expected");
            if (args.size() < *valid_argc.begin()) {
                strings.emplace_back(
                    fmt::format("at least {}", *valid_argc.begin()));
            } else if (args.size() > *valid_argc.rbegin()) {
                strings.emplace_back(
                    fmt::format("at most {}", *valid_argc.begin()));
            } else {
                strings.emplace_back(
                    fmt::format("one of {}", fmt::join(valid_argc, ",")));
            }
            strings.emplace_back(
                fmt::format("arguments. But got {}.", args.size()));

            if (!module->valid_arguments.usage.empty()) {
                strings.emplace_back(
                    fmt::format("Usage: {}", module->valid_arguments.usage));
            }
            sendReplyMessage(message->message(),
                             fmt::format("{}", fmt::join(strings, " ")));
            return false;
        }
    }

    return true;
}

void TgBotApiImpl::commandHandler(const std::string_view command,
                                  const AuthContext::Flags authflags,
                                  Message::Ptr message) {
    // Find the module first.
    const auto& module = *findModulePosition(command);
    static const std::string myName = getBotUser()->username;

    // Create MessageExt object.
    SplitMessageText how = module->valid_arguments.enabled
                               ? module->valid_arguments.split_type
                               : SplitMessageText::None;
    auto ext = std::make_shared<MessageExt>(std::move(message), how);
    const auto botCommand = ext->get<MessageAttrs::BotCommand>();
    const auto target = botCommand.target;
    if (target != myName && !target.empty()) {
        DLOG(INFO) << "Ignore mismatched target: " << std::quoted(target);
        return;
    }

    if (!module->isLoaded()) {
        // Probably unloaded.
        LOG(INFO) << "Command module is unloaded: " << module->name;
        return;
    }
    if (module->function == nullptr) {
        // Just in case.
        LOG(ERROR) << "Command module does not have a function: "
                   << module->name;
        return;
    }

    const auto authRet = _auth->isAuthorized(ext->message(), authflags);
    if (!authRet) {
        // Unauthorized user, don't run the command.
        if (ext->has<MessageAttrs::User>()) {
            LOG(INFO) << fmt::format("Unauthorized command {} from {}",
                                     module->name,
                                     ext->get<MessageAttrs::User>());
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
        return;
    }

    // Partital offloading to common code.
    if (!validateValidArgs(module, ext)) {
        return;
    }

    Locale locale = Locale::Default;
    if (ext->has<MessageAttrs::User>()) {
        locale <= ext->get<MessageAttrs::User>()->languageCode;
    }

    module->function(this, std::move(ext), (*_loader).at(locale), _provider);
}

void TgBotApiImpl::startQueueConsumerThread() {
    const auto threadFn = [](Async* async) {
        while (!async->stopWorker) {
            std::unique_lock<std::mutex> lock(async->mutex);
            async->condVariable.wait(lock, [async] {
                return !async->tasks.empty() || async->stopWorker;
            });

            if (!async->tasks.empty()) {
                auto front = std::move(async->tasks.front());
                async->tasks.pop();
                lock.unlock();
                try {
                    // Wait for the task to complete
                    front.second.get();
                } catch (const TgBot::TgException& e) {
                    LOG(ERROR) << fmt::format(
                        "[AsyncConsumer] While handling command: {}: "
                        "Exception: {}",
                        front.first, e.what());
                }
            }
        }
    };
    commandAsync.threads.emplace_back(threadFn, &commandAsync);
    queryAsync.threads.emplace_back(threadFn, &queryAsync);
}

void TgBotApiImpl::stopQueueConsumerThread() {
    const auto stopFn = [](Async* async) {
        async->stopWorker = true;
        async->condVariable.notify_all();
        for (auto& thread : async->threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    };
    stopFn(&commandAsync);
    stopFn(&queryAsync);
}

void TgBotApiImpl::addCommand(CommandModule::Ptr module) {
    auto authflags = AuthContext::Flags::REQUIRE_USER;

    if (!module->isLoaded()) {
        if (!module->load()) {
            DLOG(ERROR) << "Failed to load command module";
            return;
        }
    }
    if (!module->isEnforced()) {
        authflags |= AuthContext::Flags::PERMISSIVE;
    }

    getEvents().onCommand(module->name, [this, authflags, cmd = module->name](
                                            Message::Ptr message) {
        commandAsync.emplaceTask(
            cmd, std::async(std::launch::async, &TgBotApiImpl::commandHandler,
                            this, cmd, authflags, std::move(message)));
    });
    _modules.emplace_back(std::move(module));
}

void TgBotApiImpl::removeCommand(const std::string_view cmd) {
    const auto it = findModulePosition(cmd);
    if (it != _modules.end()) {
        _modules.erase(it);
        getEvents().onCommand(std::string(cmd), {});
        LOG(INFO) << "Removed command " << cmd;
    }
}

bool TgBotApiImpl::setBotCommands() const {
    std::vector<TgBot::BotCommand::Ptr> buffer;
    for (const auto& cmd : _modules) {
        if (!cmd->isHideDescription()) {
            auto onecommand = std::make_shared<TgBot::BotCommand>();
            onecommand->command = cmd->name;
            onecommand->description = cmd->description;
            if (cmd->isEnforced()) {
                onecommand->description += " (Owner)";
            }
            buffer.emplace_back(onecommand);
        }
    }
    try {
        getApi().setMyCommands(buffer);
    } catch (const TgBot::TgException& e) {
        LOG(ERROR) << fmt::format("Error updating bot commands list: {}",
                                  e.what());
        return false;
    }
    return true;
}

bool TgBotApiImpl::unloadCommand(const std::string_view command) {
    if (!isKnownCommand(command)) {
        LOG(INFO) << "Command " << command << " is not present";
        return false;
    }
    auto& it = *findModulePosition(command);
    it->unload();
    LOG(INFO) << "Command " << command << " unloaded";
    return true;
}

bool TgBotApiImpl::reloadCommand(const std::string_view command) {
    if (!isKnownCommand(command)) {
        LOG(INFO) << "Command " << command << " is not present";
        return false;
    }
    auto& it = *findModulePosition(command);
    if (!it->load()) {
        LOG(WARNING) << "Failed to load command " << command;
        return false;
    }
    LOG(INFO) << "Command " << command << " is loaded";
    return true;
}

bool TgBotApiImpl::isLoadedCommand(const std::string_view command) {
    if (!isKnownCommand(command)) {
        LOG(INFO) << "Command " << command << " is not present";
        return false;
    }
    return (*findModulePosition(command))->isLoaded();
}

bool TgBotApiImpl::isKnownCommand(const std::string_view command) {
    return findModulePosition(command) != _modules.end();
}

decltype(TgBotApiImpl::_modules)::iterator TgBotApiImpl::findModulePosition(
    const std::string_view command) {
    return std::ranges::find_if(
        _modules,
        [&command](const CommandModule::Ptr& e) { return e->name == command; });
}

void TgBotApiImpl::onAnyMessageFunction(const Message::Ptr& message) {
    decltype(callbacks_anycommand)::const_reverse_iterator it;
    std::vector<std::pair<std::future<AnyMessageResult>, decltype(it)>> vec;
    const std::lock_guard<std::mutex> lock(callback_anycommand_mutex);

    if (callbacks_anycommand.empty()) {
        return;
    }
    it = callbacks_anycommand.crbegin();
    while (it != callbacks_anycommand.crend()) {
        const auto fn_copy = *it;
        vec.emplace_back(std::async(std::launch::async, fn_copy, this, message),
                         it++);
    }

    for (auto& [future, callback] : vec) {
        try {
            switch (future.get()) {
                // Skip
                case TgBotApi::AnyMessageResult::Handled:
                    break;

                case TgBotApi::AnyMessageResult::Deregister:
                    callbacks_anycommand.erase(callback.base());
                    break;
            }
        } catch (const TgBot::TgException& ex) {
            LOG(ERROR) << "Error in onAnyMessageCallback: " << ex.what();
        }
    }
}

void TgBotApiImpl::onCallbackQueryFunction(
    const TgBot::CallbackQuery::Ptr& query) {
    const std::lock_guard<std::mutex> lock(callback_callbackquery_mutex);
    if (callbacks_callbackquery.empty()) {
        return;
    }
    for (const auto& [command, callback] : callbacks_callbackquery) {
        queryAsync.emplaceTask(command,
                               std::async(std::launch::async, callback, query));
    }
}

void TgBotApiImpl::startPoll() {
    const auto& botUser = getBotUser();
    LOG(INFO) << "Bot username: " << botUser->username;
    // Deleting webhook
    getApi().deleteWebhook();
    // Register -> onUnknownCommand
    getEvents().onUnknownCommand(
        [username = botUser->username](const Message::Ptr& message) {
            const auto ext = std::make_shared<MessageExt>(message);
            if (ext->get_or<MessageAttrs::BotCommand>({}).target != username) {
                return;  // ignore, unless explicitly targetted this bot.
            }
            LOG(INFO) << "Unknown command: " << message->text;
        });
    // Register -> onAnyMessage
    getEvents().onAnyMessage(
        [this](const Message::Ptr& message) { onAnyMessageFunction(message); });
    // Register->onCallbackQuery
    getEvents().onCallbackQuery([this](const TgBot::CallbackQuery::Ptr& query) {
        onCallbackQueryFunction(query);
    });
    getEvents().onChatJoinRequest([this](TgBot::ChatJoinRequest::Ptr ptr) {
        auto markup = std::make_shared<TgBot::InlineKeyboardMarkup>();
        markup->inlineKeyboard.resize(1);
        markup->inlineKeyboard.at(0).resize(2);
        markup->inlineKeyboard.at(0).at(0) =
            std::make_shared<TgBot::InlineKeyboardButton>();
        markup->inlineKeyboard.at(0).at(0)->text = "✅ Approve user";
        markup->inlineKeyboard.at(0).at(0)->callbackData =
            fmt::format("chatjoin_{}_approve", ptr->date);
        markup->inlineKeyboard.at(0).at(1) =
            std::make_shared<TgBot::InlineKeyboardButton>();
        markup->inlineKeyboard.at(0).at(1)->text = "❌ Kick user";
        markup->inlineKeyboard.at(0).at(1)->callbackData =
            fmt::format("chatjoin_{}_disapprove", ptr->date);
        std::string bio;
        if (!ptr->bio.empty()) {
            bio = fmt::format("\nTheir Bio: '{}'", ptr->bio);
        }
        sendMessage(
            ptr->chat,
            fmt::format("A new chat join request by {}{}", ptr->from, bio),
            std::move(markup));
        joinReqs.emplace_back(std::move(ptr));
    });
    onCallbackQuery(
        "__builtin_chatjoinreq_handler__",
        [this](const TgBot::CallbackQuery::Ptr& query) {
            absl::string_view queryData = query->data;
            if (!absl::ConsumePrefix(&queryData, "chatjoin_")) {
                return;
            }
            auto reqIt = std::ranges::find_if(
                joinReqs, [queryData](const TgBot::ChatJoinRequest::Ptr& req) {
                    return absl::StartsWith(queryData,
                                            fmt::to_string(req->date));
                });
            if (reqIt != joinReqs.end()) {
                const auto& request = *reqIt;
                DCHECK(absl::ConsumePrefix(&queryData,
                                           fmt::format("{}_", request->date)))
                    << "Should be able to consume";
                if (queryData == "approve") {
                    LOG(INFO) << fmt::format("Approving {} in chat {}",
                                             request->from, request->chat);
                    getApi().approveChatJoinRequest(request->chat->id,
                                                    request->from->id);
                    answerCallbackQuery(query->id, "Approved user");
                } else if (queryData == "disapprove") {
                    LOG(INFO) << fmt::format("Unapproving {} in chat {}",
                                             request->from, request->chat);
                    getApi().declineChatJoinRequest(request->chat->id,
                                                    request->from->id);
                    answerCallbackQuery(query->id, "Disapproved user");
                } else {
                    LOG(ERROR) << "Invalid payload: " << query->data;
                }
                joinReqs.erase(reqIt);
            }
        });
    getEvents().onInlineQuery([this](const TgBot::InlineQuery::Ptr& query) {
        const std::lock_guard m(callback_result_mutex);

        if (queryResults.empty()) {
            return;
        }
        AuthContext::Flags flags = AuthContext::Flags::REQUIRE_USER;
        bool canDoPrivileged = false;
        canDoPrivileged = _auth->isAuthorized(query->from, flags);
        if (!canDoPrivileged) {
            flags |= AuthContext::Flags::PERMISSIVE;
            bool canDoNonPrivileged = _auth->isAuthorized(query->from, flags);
            if (!canDoNonPrivileged) {
                return;  // no permission to answer.
            }
        }
        std::vector<TgBot::InlineQueryResult::Ptr> inlineResults;
        std::ranges::for_each(
            queryResults, [&query, &inlineResults, canDoPrivileged](auto&& x) {
                absl::string_view suffix = query->query;
                if (!canDoPrivileged && x.first.enforced) {
                    return;  // Skip this.
                }
                if (absl::ConsumePrefix(&suffix, x.first.name)) {
                    std::string arg(suffix);
                    if (x.first.hasMoreArguments) {
                        absl::StripLeadingAsciiWhitespace(&arg);
                    }
                    auto vec = x.second(std::string_view(suffix.data()));
                    inlineResults.insert(inlineResults.end(), vec.begin(),
                                         vec.end());
                }
            });
        if (inlineResults.empty()) {
            static int articleCount = 0;
            for (const auto& queryCallbacks : queryResults) {
                auto article =
                    std::make_shared<TgBot::InlineQueryResultArticle>();
                article->id = fmt::format("article-{}", articleCount++);
                article->title =
                    fmt::format("Query: {}", queryCallbacks.first.name);
                article->description = queryCallbacks.first.description;
                auto content =
                    std::make_shared<TgBot::InputTextMessageContent>();
                content->messageText = queryCallbacks.first.description;
                article->inputMessageContent = content;
                inlineResults.emplace_back(std::move(article));
            }
        }
        getApi().answerInlineQuery(query->id, inlineResults);
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
    // Start two async consumers.
    startQueueConsumerThread();
    startQueueConsumerThread();
}

TgBotApiImpl::~TgBotApiImpl() {
    callbacks_anycommand.clear();
    callbacks_callbackquery.clear();
    stopQueueConsumerThread();
    _modules.clear();
}
