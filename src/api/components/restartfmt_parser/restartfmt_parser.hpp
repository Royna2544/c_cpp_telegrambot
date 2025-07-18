
#include <absl/status/status.h>

#include <api/MessageExt.hpp>
#include <api/TgBotApi.hpp>
#include <limits>
#include <string>

#include "api/typedefs.h"

class RestartFmt {
   public:
    struct Type {
        ChatId chat_id{};
        MessageId message_id{};
        MessageThreadId message_thread_id{};

        explicit Type(const absl::string_view string);
        explicit Type(const Message::Ptr& message);
        std::string to_string() const;
        bool operator==(const Type& other) const;
    };
    using data_type = Type;

    static bool checkEnvAndVerifyRestart(TgBotApi::CPtr api);
    static bool isRestartedByThisMessage(const MessageExt::Ptr& message);

    constexpr static std::string_view ENV_VAR_NAME = "RESTART";

    // typical chatid:int32_max
    constexpr static size_t MAX_KNOWN_LENGTH =
        sizeof("RESTART=-00000000000:") +
        std::numeric_limits<MessageId>::digits10 + sizeof(":") +
        std::numeric_limits<MessageThreadId>::digits10;
};