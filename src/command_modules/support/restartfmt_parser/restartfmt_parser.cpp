#include "restartfmt_parser.hpp"

#include <absl/log/log.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>
#include <fmt/format.h>
#include <tgbot/TgException.h>

#include <TryParseStr.hpp>
#include <stdexcept>

#include "utils/Env.hpp"

RestartFmt::Type::Type(const std::string_view string) {
    std::vector<std::string> parts =
        absl::StrSplit(string, ":", absl::SkipEmpty());
    if (parts.size() != 3) {
        LOG(ERROR) << "Invalid format for RESTART=" << string
                   << " (Expected three parts)";
        throw std::invalid_argument("Invalid format for RESTART");
    }
    if (!(try_parse(parts[0], &chat_id) && try_parse(parts[1], &message_id) &&
          try_parse(parts[2], &message_thread_id))) {
        LOG(ERROR) << "Invalid format for RESTART=" << string
                   << " (Failed to parse chat_id and message_id)";
        throw std::invalid_argument("Invalid format for RESTART");
    }
}

RestartFmt::Type::Type(const Message::Ptr& message)
    : chat_id(message->chat->id),
      message_id(message->messageId),
      message_thread_id(message->messageThreadId) {}

bool RestartFmt::Type::operator==(const Type& other) const {
    return chat_id == other.chat_id && message_id == other.message_id &&
           message_thread_id == other.message_thread_id;
}

std::string RestartFmt::Type::to_string() const {
    return fmt::format("{}:{}:{}", chat_id, message_id, message_thread_id);
}

bool RestartFmt::checkEnvAndVerifyRestart(TgBotApi::CPtr api) {
    // Check if the environment variable is set and valid
    DLOG(INFO) << "RestartFmt::checkEnvAndVerifyRestart";
    auto env = Env()[ENV_VAR_NAME];
    if (!env.has()) {
        DLOG(INFO) << fmt::format("ENV_VAR {} is not set", ENV_VAR_NAME);
        return false;
    }
    std::string value = env.get();
    DLOG(INFO) << fmt::format("GETENV {}: is set to {}", ENV_VAR_NAME, value);
    LOG(INFO) << "Restart successful";
    try {
        Type t{value};
        api->sendReplyMessage(t.chat_id, t.message_id, t.message_thread_id,
                              "Restart success!");
    } catch (const std::invalid_argument& ex) {
        LOG(ERROR) << "Invalid format for RESTART=" << value << ": "
                   << ex.what();
        return false;
    } catch (const TgBot::TgException& ex) {
        LOG(ERROR) << "Failed to send message: " << ex.what();
        return false;
    }
    return true;
}

bool RestartFmt::isRestartedByThisMessage(const MessageExt::Ptr& message) {
    auto env = Env()[ENV_VAR_NAME];
    if (!env.has()) {
        DLOG(INFO) << fmt::format("ENV_VAR {} is not set", ENV_VAR_NAME);
        return false;
    }
    std::string value = env.get();
    DLOG(INFO) << fmt::format("GETENV {}: is set to {}", ENV_VAR_NAME, value);
    if (Type{message->message()} == Type{value}) {
        DLOG(INFO) << "bot is restarted by this message";
        return true;
    }
    DLOG(INFO) << "bot is not restarted by this message";
    return false;
}