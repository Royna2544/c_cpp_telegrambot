#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>
#include <string_view>

#include "api/MessageExt.hpp"

extern "C" {
const char *calculate_string(const char *expr);
void calculate_string_free(const char *expr);
}

DECLARE_COMMAND_HANDLER(calc) {
    if (message->has<MessageAttrs::ExtraText>()) {
        auto expr = message->get<MessageAttrs::ExtraText>();
        const char *result = calculate_string(expr.c_str());
        if (result == nullptr) {
            // The Rust side returns null when the result cannot be turned into
            // a C string (e.g. it contains an interior NUL byte). Constructing
            // a string_view from null is UB, so bail out gracefully.
            api->sendReplyMessage(message->message(),
                                  res->get(Strings::CALC_USAGE));
            return;
        }
        api->sendReplyMessage(message->message(), std::string_view(result));
        calculate_string_free(result);
    } else {
        api->sendReplyMessage(message->message(), res->get(Strings::CALC_USAGE));
    }
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "calc",
    .description = "Calculate a string",
    .function = COMMAND_HANDLER_NAME(calc),
};
