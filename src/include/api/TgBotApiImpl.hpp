#pragma once

#include <Authorization.h>
#include <TgBotPPImpl_shared_depsExports.h>
#include <Types.h>
#include <absl/log/check.h>
#include <tgbot/tgbot.h>

#include <CompileTimeStringConcat.hpp>
#include <InstanceClassBase.hpp>
#include <ReplyParametersExt.hpp>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string_view>
#include <vector>

#include "CommandModule.hpp"
#include "MessageExt.hpp"
#include "TgBotApi.hpp"

using TgBot::Animation;
using TgBot::Api;
using TgBot::Bot;
using TgBot::BotCommand;
using TgBot::Chat;
using TgBot::ChatPermissions;
using TgBot::EventBroadcaster;
using TgBot::File;
using TgBot::GenericReply;
using TgBot::InputFile;
using TgBot::InputSticker;
using TgBot::Message;
using TgBot::Sticker;
using TgBot::StickerSet;
using TgBot::TgLongPoll;
using TgBot::User;

// A class to effectively wrap TgBot::Api to stable interface
// This class owns the Bot instance, and users of this code cannot directly
// access it.
class TgBotPPImpl_shared_deps_API TgBotApiImpl
    : public InstanceClassBase<TgBotApiImpl>,
      public TgBotApi {
   public:
    // Constructor requires a bot token to create a Bot instance.
    explicit TgBotApiImpl(const std::string& token);
    ~TgBotApiImpl() override;

   private:
    Message::Ptr sendMessage_impl(ChatId chatId, const std::string& text,
                                  ReplyParametersExt::Ptr replyParameters,
                                  GenericReply::Ptr replyMarkup,
                                  const std::string& parseMode) const override;

    Message::Ptr sendAnimation_impl(
        ChatId chatId, boost::variant<InputFile::Ptr, std::string> animation,
        const std::string& caption,
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const override;

    Message::Ptr sendSticker_impl(
        ChatId chatId, boost::variant<InputFile::Ptr, std::string> sticker,
        ReplyParametersExt::Ptr replyParameters) const override;

    Message::Ptr editMessage_impl(
        const Message::Ptr& message, const std::string& newText,
        const TgBot::InlineKeyboardMarkup::Ptr& markup,
        const std::string& parseMode) const override;

    Message::Ptr editMessageMarkup_impl(
        const StringOrMessage& message,
        const GenericReply::Ptr& markup) const override;

    // Copy a message
    MessageId copyMessage_impl(
        ChatId fromChatId, MessageId messageId,
        ReplyParametersExt::Ptr replyParameters = nullptr) const override;

    bool answerCallbackQuery_impl(const std::string& callbackQueryId,
                                  const std::string& text = "",
                                  bool showAlert = false) const override {
        return getApi().answerCallbackQuery(callbackQueryId, text, showAlert);
    }

    void deleteMessage_impl(const Message::Ptr& message) const override;

    // Delete a range of messages
    void deleteMessages_impl(
        ChatId chatId, const std::vector<MessageId>& messageIds) const override;

    // Mute a chat member
    void restrictChatMember_impl(ChatId chatId, UserId userId,
                                 TgBot::ChatPermissions::Ptr permissions,
                                 std::uint32_t untilDate) const override;

    // Send a file to the chat
    Message::Ptr sendDocument_impl(
        ChatId chatId, FileOrString document, const std::string& caption,
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const override;

    // Send a photo to the chat
    Message::Ptr sendPhoto_impl(
        ChatId chatId, FileOrString photo, const std::string& caption,
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const override;

    // Send a video to the chat
    Message::Ptr sendVideo_impl(
        ChatId chatId, FileOrString video, const std::string& caption,
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const override;

    Message::Ptr sendDice_impl(ChatId chatId) const override;

    StickerSet::Ptr getStickerSet_impl(
        const std::string& setName) const override;

    bool createNewStickerSet_impl(
        std::int64_t userId, const std::string& name, const std::string& title,
        const std::vector<InputSticker::Ptr>& stickers,
        Sticker::Type stickerType) const override;

    File::Ptr uploadStickerFile_impl(
        std::int64_t userId, InputFile::Ptr sticker,
        const std::string& stickerFormat) const override;

    bool downloadFile_impl(const std::filesystem::path& destfilename,
                           const std::string& fileid) const override;

    /**
     * @brief Retrieves the bot's user object.
     *
     * This function retrieves the user object associated with the bot. The
     * user object contains information about the bot's account, such as its
     * username, first name, and last name.
     *
     * @return A shared pointer to the bot's user object.
     */
    User::Ptr getBotUser_impl() const override;

    bool pinMessage_impl(Message::Ptr message) const override;
    bool unpinMessage_impl(Message::Ptr message) const override;
    bool banChatMember_impl(const Chat::Ptr& chat,
                            const User::Ptr& user) const override;
    bool unbanChatMember_impl(const Chat::Ptr& chat,
                              const User::Ptr& user) const override;
    User::Ptr getChatMember_impl(ChatId chat, UserId user) const override;

    // A global link preview options
    TgBot::LinkPreviewOptions::Ptr globalLinkOptions;

   public:
    // Add commands/Remove commands
    void addCommand(CommandModule::Ptr module);
    // Remove a command from being handled
    void removeCommand(const std::string& cmd);

    void setDescriptions(const std::string& description,
                         const std::string& shortDescription) {
        getApi().setMyDescription(description);
        getApi().setMyShortDescription(shortDescription);
    }

    [[nodiscard]] bool setBotCommands() const;

    [[nodiscard]] std::string getCommandModulesStr() const override;

    void startPoll();

    bool unloadCommand(const std::string& command) override;
    bool reloadCommand(const std::string& command) override;
    bool isLoadedCommand(const std::string& command);
    bool isKnownCommand(const std::string& command);
    void commandHandler(const std::string_view command,
                        AuthContext::Flags authflags, Message::Ptr message);
    bool validateValidArgs(const CommandModule::Ptr& module,
                           MessageExt::Ptr& message);

    template <unsigned Len>
    static consteval auto getInitCallNameForClient(const char (&str)[Len]) {
        return StringConcat::cat("Register onAnyMessage callbacks: ", str);
    }

   private:
    // Protect callbacks
    std::mutex callback_anycommand_mutex;
    std::mutex callback_callbackquery_mutex;
    std::mutex callback_result_mutex;
    // A callback list where it is called when any message is received
    std::vector<AnyMessageCallback> callbacks_anycommand;
    std::multimap<std::string, TgBot::EventBroadcaster::CallbackQueryListener>
        callbacks_callbackquery;
    std::map<InlineQuery, InlineCallback> queryResults;

   public:
    void addInlineQueryKeyboard(InlineQuery query,
                                TgBot::InlineQueryResult::Ptr result) override {
        const std::lock_guard _(callback_result_mutex);
        queryResults[std::move(query)] =
            [result = std::move(result)](const std::string_view /*unused*/) {
                return std::vector{result};
            };
    }
    void addInlineQueryKeyboard(InlineQuery query,
                                InlineCallback result) override {
        const std::lock_guard _(callback_result_mutex);
        queryResults[std::move(query)] = std::move(result);
    }
    void removeInlineQueryKeyboard(const std::string& key) override {
        const std::lock_guard _(callback_result_mutex);
        for (auto it = queryResults.begin(); it != queryResults.end(); ++it) {
            if (it->first.name == key) {
                queryResults.erase(it);
                break;
            }
        }
    }

    /**
     * @brief Registers a callback function to be called when any message is
     * received.
     *
     * @param callback The function to be called when any message is
     * received.
     */
    void onAnyMessage(const AnyMessageCallback& callback) override {
        const std::lock_guard<std::mutex> _(callback_anycommand_mutex);
        callbacks_anycommand.emplace_back(callback);
    }

    void onCallbackQuery(const std::string& command,
                         const TgBot::EventBroadcaster::CallbackQueryListener&
                             listener) override {
        const std::lock_guard<std::mutex> _(callback_callbackquery_mutex);
        callbacks_callbackquery.emplace(command, listener);
    }

   private:
    [[nodiscard]] EventBroadcaster& getEvents() { return _bot.getEvents(); }
    [[nodiscard]] const Api& getApi() const { return _bot.getApi(); }

    struct Async {
        // A flag to stop CallbackQuery worker
        std::atomic<bool> stopWorker = false;
        // A queue to handle command (commandname, async future)
        std::queue<std::pair<std::string, std::future<void>>> tasks;
        // mutex to protect shared queue
        std::mutex mutex;
        // condition variable to wait for async tasks to finish.
        std::condition_variable condVariable;
        // worker thread(s) to consume command queue
        std::vector<std::thread> threads;

        void emplaceTask(const std::string& command,
                         std::future<void>&& future) {
            std::unique_lock<std::mutex> lock(mutex);
            tasks.emplace(std::forward<const std::string&>(command),
                          std::forward<std::future<void>>(future));
            condVariable.notify_one();
        }

    } queryAsync, commandAsync;

    std::vector<std::unique_ptr<CommandModule>> _modules;
    Bot _bot;
    decltype(_modules)::iterator findModulePosition(const std::string_view command);
    void onAnyMessageFunction(const Message::Ptr& message);
    void onCallbackQueryFunction(const TgBot::CallbackQuery::Ptr& query);
    void startQueueConsumerThread();
    void stopQueueConsumerThread();
};
