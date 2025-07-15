#include <absl/log/log.h>
#include <tgbot/TgException.h>
#include <trivial_helpers/_std_chrono_templates.h>
#include <trivial_helpers/_tgbot.h>

#include <chrono>
#include <condition_variable>
#include <global_handlers/SpamBlockManager.hpp>
#include <mutex>
#include "global_handlers/SpamBlock.hpp"

void SpamBlockManager::runFunction(const std::stop_token &token) {
    while (!token.stop_requested()) {
        consumeAndDetect();
        std::unique_lock<std::mutex> lock(mutex);
        condvar.wait_for(lock, sSpamDetectDelay,
                         [token] { return token.stop_requested(); });
    }
}

void SpamBlockManager::onPreStop() {
    { std::lock_guard<std::mutex> lock(mutex); }
    // Cancel the timer
    condvar.notify_all();
}

void SpamBlockManager::onMessageAdded(const size_t count) {
    if (count < sImmediateStartThreshold) {
        return;
    }
    { std::lock_guard<std::mutex> lock(mutex); }
    LOG_EVERY_N_SEC(INFO, sSpamDetectDelay.count()) << "Messages queued: " << count << ". Starting thread now";
    // Wake up the timer
    condvar.notify_all();
}

void SpamBlockManager::onDetected(ChatId chat, UserId user,
                                  std::vector<MessageId> messageIds) const {
    // Initial set - all false set
    static auto perms = std::make_shared<TgBot::ChatPermissions>();
    switch (getConfig()) {
        case Config::PURGE_AND_MUTE:
            LOG(INFO) << fmt::format("Try mute offending user");
            try {
                _api->muteChatMember(
                    chat, user, perms,
                    std::chrono::system_clock::now() + kMuteDuration);
            } catch (const TgBot::TgException &e) {
                LOG(WARNING) << fmt::format("Cannot mute: {}", e.what());
            }
            [[fallthrough]];
        case Config::PURGE: {
            try {
                _api->deleteMessages(chat, messageIds);
            } catch (const TgBot::TgException &e) {
                DLOG(INFO) << "Error deleting messages: " << e.what();
            }
            [[fallthrough]];
        }
        case Config::LOGGING_ONLY:
            SpamBlockBase::onDetected(chat, user, messageIds);
            break;
        default:
            break;
    };
}

bool SpamBlockManager::shouldBeSkipped(const Message::Ptr &message) const {
    if (_auth->isAuthorized(message, AuthContext::AccessLevel::AdminUser)) {
        return true;
    }

    // Ignore old messages
    if (!AuthContext::isUnderTimeLimit(message)) {
        return true;
    }

    // Bot's PM is not a concern
    if (message->chat->type == TgBot::Chat::Type::Private) {
        return true;
    }

    // Allow photos to be sent
    if (!message->photo.empty()) {
        return true;
    }

    return false;
}

SpamBlockManager::SpamBlockManager(TgBotApi::Ptr api, AuthContext *auth)
    : _api(api), _auth(auth) {
    api->onAnyMessage([this](TgBotApi::CPtr, const Message::Ptr &message) {
        addMessage(message);
        return TgBotApi::AnyMessageResult::Handled;
    });
    run();
}
