#include "restartfmt_parser.hpp"

#include <absl/log/log.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>

#include <TryParseStr.hpp>

#include "absl/status/status.h"

std::optional<RestartFmt::data_type> RestartFmt::fromString(
    const std::string& string, bool withPrefix) {
    std::string_view view = string;
    ChatId parsedChatId = 0;
    MessageId parsedMessageId = 0;

    if (!absl::ConsumePrefix(&view, "RESTART=") && withPrefix) {
        LOG(ERROR) << "Invalid format for RESTART=" << string
                   << " (Doesn't have the expected prefix)";
        return std::nullopt;
    }
    std::vector<std::string> parts =
        absl::StrSplit(view, ":", absl::SkipEmpty());
    if (parts.size() != 2) {
        LOG(ERROR) << "Invalid format for RESTART=" << string
                   << " (Expected two parts)";
        return std::nullopt;
    }
    if (try_parse(parts[0], &parsedChatId) &&
        try_parse(parts[1], &parsedMessageId)) {
        return std::make_pair(parsedChatId, parsedMessageId);
    } else {
        LOG(ERROR) << "Invalid format for RESTART=" << string
                   << " (Failed to parse chat_id and message_id)";
        return std::nullopt;
    }
}

std::optional<RestartFmt::data_type> RestartFmt::fromEnvVar() {
    const char* restartStr = getenv(ENV_VAR_NAME);  // NOLINT
    if (restartStr == nullptr) {
        LOG(WARNING) << "No " << ENV_VAR_NAME << " environment variable found";
        return std::nullopt;
    }
    return fromString(restartStr, false);
}

std::string RestartFmt::toString(const data_type& data, bool withPrefix) {
    std::string dataString = absl::StrCat(std::to_string(data.first), ":",
                                          std::to_string(data.second));

    if (withPrefix) {
        return absl::StrCat("RESTART=", dataString);
    } else {
        return dataString;
    }
}

absl::Status RestartFmt::handleMessage(ApiPtr api) {
    if (const char* env = getenv(RestartFmt::ENV_VAR_NAME); env != nullptr) {
        const auto v = RestartFmt::fromEnvVar();
        if (!v) {
            LOG(ERROR) << "Invalid restart command format: " << env;
            unsetenv(RestartFmt::ENV_VAR_NAME);
            return absl::InvalidArgumentError(
                "Invalid restart variable format");
        } else {
            LOG(INFO) << "Restart successful";

            api->sendReplyMessage(v->first, v->second, "Restart success!");
            unsetenv(RestartFmt::ENV_VAR_NAME);
            return absl::OkStatus();
        }
    }
    // Could get here when the environment variable is not set
    return absl::UnavailableError("Environment variable not set");
}