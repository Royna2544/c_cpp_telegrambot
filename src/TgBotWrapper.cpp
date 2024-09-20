#include <absl/strings/ascii.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>
#include <dlfcn.h>

#include <Random.hpp>
#include <StringResManager.hpp>
#include <TgBotWrapper.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <future>
#include <libos/OnTerminateRegistrar.hpp>
#include <libos/libsighandler.hpp>
#include <memory>
#include <queue>
#include <string_view>
#include <utility>

void MessageExt::update() {
    // Try to find botcommand entity
    const auto botCommandEnt =
        std::ranges::find_if(entities, [](const auto& entity) {
            return entity->type == TgBot::MessageEntity::Type::BotCommand;
        });

    // I believe entity must be sent here.
    if (botCommandEnt != entities.end()) {
        const auto entry = *botCommandEnt;
        LOG_IF(WARNING, entry->offset != 0)
            << "Unexpected offset: " << entry->offset;
        _extra_args = absl::StripAsciiWhitespace(
            text.substr(entry->offset + entry->length));
        auto command_string = text.substr(0, entry->length);
        command_string = absl::StripPrefix(command_string, "/");
        const auto at_pos = command_string.find('@');
        if (at_pos != std::string::npos) {
            command.name = command_string.substr(0, at_pos);
            command.target = command_string.substr(at_pos + 1);
            LOG_IF(WARNING, command.target.empty() || command.name.empty())
                << "Parsing logic error, target or name is empty";
        } else {
            command.name = command_string;
        }
    }
}

void MessageExt::update(const SplitMessageText& how) {
    _arguments.clear();
    if (_extra_args.size() == 0) {
        return;  // No arguments, nothing to split.
    }
    switch (how) {
        case CommandModule::ValidArgs::Split::ByWhitespace:
            _arguments =
                absl::StrSplit(_extra_args, ' ', absl::SkipWhitespace());
            break;
        case CommandModule::ValidArgs::Split::ByComma:
            _arguments =
                absl::StrSplit(_extra_args, ',', absl::SkipWhitespace());
            break;
        case CommandModule::ValidArgs::Split::None:
            // No-op, considering one argument.
            _arguments.emplace_back(_extra_args);
            break;
    }
}

CommandModule::CommandModule(std::filesystem::path filePath)
    : filePath(std::move(filePath)) {}

bool CommandModule::load() {
    const char* dlerrorBuf = nullptr;
    loadcmd_function_t sym = nullptr;
    const std::string cmdNameStr =
        filePath.filename().replace_extension().string();
    std::string_view cmdNameView(cmdNameStr);

    if (!absl::ConsumePrefix(&cmdNameView, "libcmd_")) {
        LOG(WARNING) << "Failed to extract command name from " << filePath;
        return false;
    }

    handle = dlopen(filePath.string().c_str(), RTLD_NOW);
    if (handle == nullptr) {
        dlerrorBuf = dlerror() ?: "unknown";
        LOG(WARNING) << "dlopen failed for " << filePath.filename() << ": "
                     << dlerrorBuf;
        return false;
    }
    sym = reinterpret_cast<loadcmd_function_cstyle_t>(
        dlsym(handle, DYN_COMMAND_SYM_STR));
    if (sym == nullptr) {
        LOG(WARNING) << "Failed to lookup symbol '" DYN_COMMAND_SYM_STR "' in "
                     << filePath;
        dlclose(handle);
        return false;
    }

    if (!sym(cmdNameView.data(), *this)) {
        LOG(WARNING) << "Failed to load command module from " << filePath;
        function = nullptr;
        dlclose(handle);
        return false;
    }
    isLoaded = true;

#ifndef NDEBUG
    Dl_info info{};
    void* fnptr = nullptr;
    if (dladdr(dlsym(handle, DYN_COMMAND_SYM_STR), &info) < 0) {
        dlerrorBuf = dlerror() ?: "unknown";
        LOG(WARNING) << "dladdr failed for " << filePath << ": " << dlerrorBuf;
    } else {
        fnptr = info.dli_saddr;
    }

    DLOG(INFO) << "Loaded RT command module from " << filePath.filename();
    DLOG(INFO) << "Module dump: { enforced: " << isEnforced()
               << ", name: " << command << ", fn: " << fnptr << " }";
#endif
    return true;
}

bool CommandModule::unload() {
    if ((handle != nullptr) && isLoaded) {
        isLoaded = false;
        function = nullptr;
        dlclose(handle);
        handle = nullptr;
        return true;
    }
    return false;
}

void TgBotWrapper::commandHandler(unsigned int authflags,
                                  MessageExt::Ptr message) {
    static const std::string myName = getBotUser()->username;
    const auto target = message->get_command().target;
    if (target != myName && !target.empty()) {
        return;
    }
    const auto& module = *findModulePosition(message->get_command().name);

    if (!module->getLoaded()) {
        // Probably unloaded.
        LOG(INFO) << "Command module is unloaded: " << module->command;
        return;
    }
    if (module->function == nullptr) {
        // Just in case.
        LOG(ERROR) << "Command module does not have a function: "
                   << module->command;
        return;
    }

    const auto authRet =
        AuthContext::getInstance()->isAuthorized(message, authflags);
    if (!authRet) {
        // Unauthorized user, don't run the command.
        if (message->from) {
            LOG(INFO) << "Unauthorized command " << module->command << " from "
                      << message->from->username
                      << " (id: " << message->from->id << ")";
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
            }
        }
        return;
    }

    // Partital offloading to common code.
    if (module->valid_arguments.enabled) {
        bool check_argc = !module->valid_arguments.counts.empty();

        // Try to split them.
        message->update(module->valid_arguments.split_type);
        const std::vector<std::string>& args = message->arguments();
        if (check_argc) {
            std::vector<int> _valid_argc = module->valid_arguments.counts;
            // Sort the valid_arguments
            std::ranges::sort(_valid_argc);
            // Push it to a set, so remove duplicates.
            std::set valid_argc(_valid_argc.begin(), _valid_argc.end());

            // Check if the number of arguments matches the expected.
            if (!std::ranges::any_of(valid_argc, [&args](int count) {
                    return args.size() == count;
                })) {
                std::stringstream ss;
                ss << "Invalid number of arguments. Expected ";
                if (args.size() < *valid_argc.begin()) {
                    ss << "at least " << *valid_argc.begin();
                } else if (args.size() > *valid_argc.rbegin()) {
                    ss << "at most " << *valid_argc.rbegin();
                } else {
                    ss << "one of ";
                    for (const auto& arg : valid_argc) {
                        ss << arg << " ";
                    }
                }
                ss << " arguments. But got " << args.size() << "." << std::endl;
                ss << "Usage: " << module->valid_arguments.usage;
                sendReplyMessage(message, ss.str());
                return;
            }
        }
    }

    module->function(shared_from_this(), std::move(message));
}

void TgBotWrapper::startQueueConsumerThread() {
    workerThreads.emplace_back([this]() {
        while (!stopWorker) {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCV.wait(lock,
                         [this] { return !asyncTasks.empty() || stopWorker; });

            if (!asyncTasks.empty()) {
                auto front = std::move(asyncTasks.front());
                asyncTasks.pop();
                lock.unlock();
                try {
                    // Wait for the task to complete
                    front.second.get();
                } catch (const TgBot::TgException& e) {
                    LOG(ERROR) << "[AsyncConsumer] While handling command: "
                               << front.first << ": " << e.what();
                }
            }
        }
    });
}

void TgBotWrapper::stopQueueConsumerThread() {
    stopWorker = true;
    queueCV.notify_all();
    for (auto& thread : workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void TgBotWrapper::addCommand(CommandModule::Ptr module) {
    unsigned int authflags = AuthContext::Flags::REQUIRE_USER;

    if (!module->getLoaded()) {
        DLOG(INFO) << "Module not loaded, trying to load it...";
        if (!module->load()) {
            DLOG(ERROR) << "Failed to load command module";
            return;
        }
    }
    if (!module->isEnforced()) {
        authflags |= AuthContext::Flags::PERMISSIVE;
    }

    getEvents().onCommand(
        module->command,
        [this, authflags, cmd = module->command](const Message::Ptr& message) {
            std::lock_guard<std::mutex> lock(queueMutex);
            asyncTasks.emplace(
                cmd, std::async(std::launch::async,
                                &TgBotWrapper::commandHandler, this, authflags,
                                std::make_shared<MessageExt>(message)));
            queueCV.notify_one();
        });
    _modules.emplace_back(std::move(module));
}

void TgBotWrapper::removeCommand(const std::string& cmd) {
    const auto it = findModulePosition(cmd);
    if (it != _modules.end()) {
        _modules.erase(it);
        getEvents().onCommand(cmd, {});
        LOG(INFO) << "Removed command " << cmd;
    }
}

bool TgBotWrapper::setBotCommands() const {
    std::vector<TgBot::BotCommand::Ptr> buffer;
    for (const auto& cmd : _modules) {
        if (!cmd->isHideDescription()) {
            auto onecommand = std::make_shared<CommandModule>(*cmd);
            if (cmd->isEnforced()) {
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
        ss << module->command << " ";
    }
    return ss.str();
}

bool TgBotWrapper::unloadCommand(const std::string& command) {
    if (!isKnownCommand(command)) {
        LOG(INFO) << "Command " << command << " is not present";
        return false;
    }
    auto& it = *findModulePosition(command);
    it->unload();
    LOG(INFO) << "Command " << command << " unloaded";
    return true;
}

bool TgBotWrapper::reloadCommand(const std::string& command) {
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

bool TgBotWrapper::isLoadedCommand(const std::string& command) {
    if (!isKnownCommand(command)) {
        LOG(INFO) << "Command " << command << " is not present";
        return false;
    }
    return (*findModulePosition(command))->getLoaded();
}

bool TgBotWrapper::isKnownCommand(const std::string& command) {
    return findModulePosition(command) != _modules.end();
}

decltype(TgBotWrapper::_modules)::iterator TgBotWrapper::findModulePosition(
    const std::string& command) {
    return std::ranges::find_if(_modules,
                                [&command](const CommandModule::Ptr& e) {
                                    return e->command == command;
                                });
}

void TgBotWrapper::startPoll() {
    getApi().deleteWebhook();
    getEvents().onUnknownCommand([](Message::Ptr message) {
        LOG(INFO) << "Unknown command: " << message->text;
    });
    getEvents().onAnyMessage([this](const Message::Ptr& message) {
        decltype(callbacks)::reverse_iterator it;
        std::vector<std::pair<std::future<AnyMessageResult>, decltype(it)>> vec;

        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            it = callbacks.rbegin();
            while (it != callbacks.rend()) {
                vec.emplace_back(
                    std::async(std::launch::async, *it, shared_from_this(),
                               std::make_shared<MessageExt>(message)),
                    it++);
            }
        }
        for (auto& [future, callback] : vec) {
            try {
                switch (future.get()) {
                    // Skip
                    case TgBotApi::AnyMessageResult::Handled:
                        break;

                    case TgBotApi::AnyMessageResult::Deregister:
                        callbacks.erase(std::next(callback).base());
                        break;
                }
            } catch (const TgBot::TgException& ex) {
                LOG(ERROR) << "Error in onAnyMessageCallback: " << ex.what();
            }
        }
    });

    OnTerminateRegistrar::getInstance()->registerCallback([this]() {
        std::ranges::for_each(
            _modules, [this](auto& elem) { unloadCommand(elem->command); });
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
        return getApi().sendMessage(chatId, text, globalLinkOptions,
                                    replyParameters, replyMarkup, parseMode,
                                    kDisableNotifications, {},
                                    ReplyParamsToMsgTid{replyParameters});

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
                                    message->messageId, "", "",
                                    globalLinkOptions, markup);
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
        replyParameters, nullptr, false, ReplyParamsToMsgTid{replyParameters});
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

bool TgBotWrapper::pinMessage_impl(Message::Ptr message) const {
    DEBUG_ASSERT_NONNULL_PARAM(message);
    return getApi().pinChatMessage(message->chat->id, message->messageId,
                                   kDisableNotifications);
}

bool TgBotWrapper::unpinMessage_impl(Message::Ptr message) const {
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

TgBotWrapper::TgBotWrapper(const std::string& token) : _bot(token) {
    globalLinkOptions = std::make_shared<TgBot::LinkPreviewOptions>();
    globalLinkOptions->isDisabled = true;
    // Start two async consumers.
    startQueueConsumerThread();
    startQueueConsumerThread();
}

TgBotWrapper::~TgBotWrapper() { stopQueueConsumerThread(); }

DECLARE_CLASS_INST(TgBotWrapper);