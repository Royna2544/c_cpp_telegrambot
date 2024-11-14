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
        const std::string_view result = calculate_string(expr.c_str());
        api->sendReplyMessage(message->message(), result);
        calculate_string_free(result.data());
    } else {
        api->sendReplyMessage(message->message(), "Usage: /calc <expression>");
    }
}

extern "C" const struct DynModule DYN_COMMAND_EXPORT DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "calc",
    .description = "Calculate a string",
    .function = COMMAND_HANDLER_NAME(calc),
};