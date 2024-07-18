#include "CStringLifetime.h"
#include <TgBotWrapper.hpp>

extern "C" {
    const char *calculate_string(const char *expr);
    void calculate_string_free(const char *expr);
}

DECLARE_COMMAND_HANDLER(calc, bot, message) {
    MessageWrapper wrapper(bot, message);
    if (wrapper.hasExtraText()) {
        CStringLifetime expr = wrapper.getExtraText();
        const char *result = calculate_string(expr);
        wrapper.sendMessageOnExit(result);
        calculate_string_free(result);
    }
}

DYN_COMMAND_FN(n, module) {
    module.command = "calc";
    module.description = "Calculate a string";
    module.fn = COMMAND_HANDLER_NAME(calc);
    module.flags = CommandModule::Flags::None;
    return true;
}