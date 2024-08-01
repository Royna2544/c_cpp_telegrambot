
#include <optional>
#include <string>
#include <TgBotWrapper.hpp>
#include <absl/status/status.h>

#include "Types.h"

class RestartFmt {
   public:
    using data_type = std::pair<ChatId, MessageId>;
    // function to parse the given string and populate chat_id and message_id
    static std::optional<data_type> fromString(const std::string& string, bool withPrefix = false);

    // parse the string from environment variables
    static std::optional<data_type> fromEnvVar();

    // function to convert chat_id and message_id to a string
    static std::string toString(const data_type& data, bool withPrefix = false);

    static absl::Status handleMessage(ApiPtr api);

    constexpr static const char* ENV_VAR_NAME = "RESTART";
    // typical chatid:int32_max
    constexpr static size_t MAX_KNOWN_LENGTH =
        sizeof("RESTART=-00000000000:2147483647");
};