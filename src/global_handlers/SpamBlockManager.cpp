#include <tgbot/TgException.h>
#include <trivial_helpers/_std_chrono_templates.h>
#include <trivial_helpers/_tgbot.h>

#include <chrono>
#include <global_handlers/SpamBlockManager.hpp>
#include <thread>

void SpamBlockManager::runFunction(const std::stop_token &token) {
    while (!token.stop_requested()) {
        consumeAndDetect();
        std::this_thread::sleep_for(sSpamDetectDelay);
    }
}

void SpamBlockManager::onDetected(ChatId chat, UserId user,
                                  std::vector<MessageId> messageIds) const {
    // Initial set - all false set
    static auto perms = std::make_shared<TgBot::ChatPermissions>();
    switch (_config) {
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
    if (_auth->isAuthorized(message, AuthContext::Flags::None)) {
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