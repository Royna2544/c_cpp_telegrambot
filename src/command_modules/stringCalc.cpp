#include <TgBotWrapper.hpp>

#include "CStringLifetime.h"

extern "C" {
const char *calculate_string(const char *expr);
void calculate_string_free(const char *expr);
}

DECLARE_COMMAND_HANDLER(calc, bot, message) {
    if (message->has<MessageExt::Attrs::ExtraText>()) {
        CStringLifetime expr = message->text;
        const char *result = calculate_string(expr);
        bot->sendReplyMessage(message, result);
        calculate_string_free(result);
    }
}

DYN_COMMAND_FN(n, module) {
    module.command = "calc";
    module.description = "Calculate a string";
    module.function = COMMAND_HANDLER_NAME(calc);
    module.flags = CommandModule::Flags::None;
    return true;
}