#include "ChatDataCollector.hpp"

#include <absl/log/log.h>
#include <absl/strings/str_split.h>
#include <fmt/format.h>

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
    std::string text;
    if (message->text) {
        text = *message->text;
    } else if (message->caption) {
        text = *message->caption;
    }
    textLength = text.size();
    std::vector<std::string> words = absl::StrSplit(text, ' ');
    wordCount = words.size();
}

void ChatDataCollector::onMessage(const Message::Ptr& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!message->from || !message->chat) {
        return;  // Skip messages with no sender or chat
    }
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

    // Update user dict
    userDict_[message->from->id] = message->from;
    // Update chat dict
    chatDict_[message->chat->id] = message->chat;
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
                        "message_id,reply_to_chat_id,thread_id,is_premium,text_"
                        "length,word_count\n";
    }

    if (std::filesystem::exists("user_dict.csv")) {
        // Load existing user dict
        std::ifstream userDictFile("user_dict.csv");
        std::string line;
        std::getline(userDictFile, line);  // Skip header
        while (std::getline(userDictFile, line)) {
            std::vector<std::string> parts = absl::StrSplit(line, ',');
            if (parts.size() < 5) {
                continue;  // Malformed line
            }
            User::Ptr user = std::make_shared<User>();
            user->id = std::stoll(parts[0]);
            user->firstName = parts[1];
            if (!parts[2].empty()) {
                user->lastName = parts[2];
            }
            if (!parts[3].empty()) {
                user->username = parts[3];
            }
            user->isPremium = (parts[4] == "1");
            userDict_[user->id] = user;
        }
    }

    if (std::filesystem::exists("chat_dict.csv")) {
        // Load existing chat dict
        std::ifstream chatDictFile("chat_dict.csv");
        std::string line;
        std::getline(chatDictFile, line);  // Skip header
        while (std::getline(chatDictFile, line)) {
            std::vector<std::string> parts = absl::StrSplit(line, ',');
            if (parts.size() < 2) {
                continue;  // Malformed line
            }
            Chat::Ptr chat = std::make_shared<Chat>();
            chat->id = std::stoll(parts[0]);
            if (!parts[1].empty()) {
                chat->title = parts[1];
            }
            chatDict_[chat->id] = chat;
        }
    }

    api->onAnyMessage(
        [this](TgBotApi::CPtr /*api*/, const Message::Ptr& message) {
            onMessage(message);
            return TgBotApi::AnyMessageResult::Handled;
        });
    api->onEditedMessage(
        [this](const Message::Ptr& message) { onMessage(message); });
}

ChatDataCollector::~ChatDataCollector() {
    // Write user dict
    std::ofstream userDictFile("user_dict.csv");
    userDictFile << "user_id,first_name,last_name,username,is_premium\n";
    for (const auto& [userId, userPtr] : userDict_) {
        userDictFile << fmt::format(
            "{},{},{},{},{}\n", userId, userPtr->firstName,
            userPtr->lastName.value_or(""), userPtr->username.value_or(""),
            userPtr->isPremium && *userPtr->isPremium ? "1" : "0");
    }

    // Write chat dict
    std::ofstream chatDictFile("chat_dict.csv");
    chatDictFile << "chat_id,chat_title\n";
    for (const auto& [chatId, chatPtr] : chatDict_) {
        chatDictFile << fmt::format("{},{}\n", chatId,
                                    chatPtr->title.value_or(""));
    }
}