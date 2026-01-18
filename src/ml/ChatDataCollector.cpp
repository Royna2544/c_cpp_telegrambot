#include "ChatDataCollector.hpp"

#include <absl/log/log.h>

#include <filesystem>
#include <fstream>

ChatDataCollector::Data::Data(const Message::Ptr& message) {
    if (message->text) {
        msgType = Data::MsgType::TEXT;
    } else if (!message->photo.empty()) {
        msgType = Data::MsgType::PHOTO;
    } else if (message->video) {
        msgType = Data::MsgType::VIDEO;
    } else if (message->sticker) {
        msgType = Data::MsgType::STICKER;
    } else if (message->animation) {
        msgType = Data::MsgType::GIF;
    } else if (message->document) {
        msgType = Data::MsgType::DOCUMENT;
    } else {
        msgType = Data::MsgType::ETC;
    }
    chatId = message->chat->id;
    userId = message->from->id;
    timestamp = message->date;
    isEdited = message->editDate.has_value();
    isForwarded = message->forwardOrigin != nullptr;
    replyToUserId =
        message->replyToMessage ? message->replyToMessage->from->id : 0;
    messageid = message->messageId;
    replyToMessageId =
        message->replyToMessage ? message->replyToMessage->messageId : 0;
    replyToChatId =
        message->replyToMessage ? message->replyToMessage->chat->id : 0;
    threadId = message->messageThreadId.has_value()
                   ? message->messageThreadId.value()
                   : 0;
    is_premium = message->from->isPremium && message->from->isPremium.value();
}

void ChatDataCollector::onMessage(const Message::Ptr& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (message->isAutomaticForward) {
        return;  // Skip automatic forwards from channel
    }
    if (message->from->isBot) {
        return;  // Skip messages from bots
    }
    if (!message->newChatMembers.empty() || message->leftChatMember ||
        message->groupChatCreated || message->pinnedMessage) {
        return;  // Skip join/leave messages
    }
    if (message->senderChat &&
        message->senderChat->type == Chat::Type::Channel) {
        return;  // Skip messages sent on behalf of a channel
    }
    constexpr UserId kExcludedUserId = 777000;  // Telegram official account
    if (message->from->id == kExcludedUserId) {
        return;  // Skip messages from Telegram official account
    }
    if (message->chat->type != Chat::Type::Supergroup) {
        return;  // Only collect from supergroups
    }
    chatDataFile << Data(message);
}

ChatDataCollector::ChatDataCollector(TgBotApi::Ptr api) {
    bool existed = false;
    constexpr std::string_view kChatDataFile = "chat_data.csv";

    // Write chat data to chat_data.csv
    if (std::filesystem::exists(kChatDataFile)) {
        // Then skip writing header
        existed = true;
    }
    if (existed) {
        chatDataFile.open(kChatDataFile.data(), std::ios::app);
    } else {
        chatDataFile.open(kChatDataFile.data());
    }
    if (!existed) {
        chatDataFile << "chat_id,user_id,timestamp,message_type,is_edited,is_"
                        "forwarded,reply_to_user_id,message_id,reply_to_"
                        "message_id,reply_to_chat_id,thread_id,is_premium\n";
    }
    api->onAnyMessage(
        [this](TgBotApi::CPtr /*api*/, const Message::Ptr& message) {
            onMessage(message);
            return TgBotApi::AnyMessageResult::Handled;
        });
    api->onEditedMessage(
        [this](const Message::Ptr& message) { onMessage(message); });
}

ChatDataCollector::~ChatDataCollector() = default;