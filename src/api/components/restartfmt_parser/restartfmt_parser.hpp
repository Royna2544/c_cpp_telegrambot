
#include <absl/status/status.h>

#include <api/types/ParsedMessage.hpp>
#include <api/TgBotApi.hpp>
#include <limits>
#include <string>

class RestartFmt {
   public:
    struct Type {
        api::types::Chat::id_type chat_id{};
        api::types::Message::messageId_type message_id{};
        api::types::Message::messageThreadId_type message_thread_id{};

        explicit Type(const absl::string_view string);
        explicit Type(const api::types::ParsedMessage& message);
        std::string to_string() const;
        bool operator==(const Type& other) const;
    };
    using data_type = Type;

    static bool checkEnvAndVerifyRestart(TgBotApi::CPtr api);
    static bool isRestartedByThisMessage(
        const api::types::ParsedMessage& message);

    constexpr static std::string_view ENV_VAR_NAME = "RESTART";

    // typical chatid:int32_max
    constexpr static size_t MAX_KNOWN_LENGTH =
        sizeof("RESTART=-00000000000:") +
        std::numeric_limits<api::types::Message::messageId_type>::digits10 +
        sizeof(":") +
        std::numeric_limits<
            api::types::Message::messageThreadId_type>::digits10;
};