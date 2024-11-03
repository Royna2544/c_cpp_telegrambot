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
    }
}

DYN_COMMAND_FN(n, module) {
    module.name = "calc";
    module.description = "Calculate a string";
    module.function = COMMAND_HANDLER_NAME(calc);
    module.flags = CommandModule::Flags::None;
    return true;
}