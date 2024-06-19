#include <BotReplyMessage.h>

#include "CStringLifetime.h"
#include "cmd_dynamic.h"
#include "command_modules/CommandModule.h"
#include <MessageWrapper.hpp>

extern "C" {
    const char *calculate_string(const char *expr);
    void calculate_string_free(const char *expr);
}

static void cmd_stringcalc(const Bot& bot, const Message::Ptr& message) {
    MessageWrapper wrapper(bot, message);
    if (wrapper.hasExtraText()) {
        CStringLifetime expr = wrapper.getExtraText();
        const char *result = calculate_string(expr);
        wrapper.sendMessageOnExit(result);
        calculate_string_free(result);
    }
}

extern "C" {
void DYN_COMMAND_SYM (CommandModule &module) {
    module.command = "calc";
    module.description = "Calculate a string";
    module.fn = cmd_stringcalc;
    module.flags = CommandModule::Flags::None;
};
}