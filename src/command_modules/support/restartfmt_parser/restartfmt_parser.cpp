#include "restartfmt_parser.hpp"

#include <absl/log/log.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>

#include <TryParseStr.hpp>

#include "utils/Env.hpp"

std::optional<RestartFmt::data_type> RestartFmt::fromString(
    const std::string& string, bool withPrefix) {
    std::string_view view = string;
    ChatId parsedChatId = 0;
    MessageId parsedMessageId = 0;
    int parsedMessageThreadId = 0;

    if (withPrefix && !absl::ConsumePrefix(&view, "RESTART=")) {
        LOG(ERROR) << "Invalid format for RESTART=" << string
                   << " (Doesn't have the expected prefix)";
        return std::nullopt;
    }
    std::vector<std::string> parts =
        absl::StrSplit(view, ":", absl::SkipEmpty());
    if (parts.size() != 3) {
        LOG(ERROR) << "Invalid format for RESTART=" << string
                   << " (Expected three parts)";
        return std::nullopt;
    }
    if (try_parse(parts[0], &parsedChatId) &&
        try_parse(parts[1], &parsedMessageId) &&
        try_parse(parts[2], &parsedMessageThreadId)) {
        return Type{parsedChatId, parsedMessageId, parsedMessageThreadId};
    } else {
        LOG(ERROR) << "Invalid format for RESTART=" << string
                   << " (Failed to parse chat_id and message_id)";
        return std::nullopt;
    }
}

std::optional<RestartFmt::data_type> RestartFmt::fromEnvVar() {
    const auto restartStr = Env()[ENV_VAR_NAME];
    if (!restartStr.has()) {
        LOG(WARNING) << "No " << ENV_VAR_NAME << " environment variable found";
        return std::nullopt;
    }
    return fromString(restartStr.get(), false);
}

std::string RestartFmt::toString(const data_type& data, bool withPrefix) {
    std::string dataString = absl::StrCat(std::to_string(data.first), ":",
                                          std::to_string(data.second), ":",
                                          std::to_string(data.third));

    if (withPrefix) {
        return absl::StrCat("RESTART=", dataString);
    } else {
        return dataString;
    }
}

absl::Status RestartFmt::handleMessage(TgBotApi::CPtr api) {
    if (const auto env = Env()[ENV_VAR_NAME]; env.has()) {
        const auto v = RestartFmt::fromEnvVar();
        if (!v) {
            LOG(ERROR) << "Invalid restart command format: " << env;
            Env()[ENV_VAR_NAME].clear();
            return absl::InvalidArgumentError(
                "Invalid restart variable format");
        } else {
            LOG(INFO) << "Restart successful";
            api->sendReplyMessage(v->first, v->second, v->third,
                                  "Restart success!");
            Env()[ENV_VAR_NAME].clear();
            return absl::OkStatus();
        }
    }
    // Could get here when the environment variable is not set
    return absl::UnavailableError("Environment variable not set");
}