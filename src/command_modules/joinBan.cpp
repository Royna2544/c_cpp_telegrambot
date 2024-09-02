#include <TgBotWrapper.hpp>

#include "internal/_tgbot.h"

DECLARE_COMMAND_HANDLER(joinban, api, message) {
    MessageWrapper wrapper(api, message);

    if (!wrapper.hasExtraText()) {
        wrapper.sendMessageOnExit("Please provide a keyword to ban");
        return;
    }

    const auto ban_keyword = wrapper.getExtraText();

    api->sendMessage(
        message, "I will ban new users with name containing: " + ban_keyword);

    api->registerCallback([ban_keyword](ApiPtr api, MessagePtr message) {
        if (const auto& newUsers = message->newChatMembers; !newUsers.empty()) {
            for (const auto& user : newUsers) {
                if (UserPtr_toString(user).find(ban_keyword) !=
                    std::string::npos) {
                    api->sendMessage(message->chat->id,
                                     "User banned: @" + user->username);
                    break;
                }
            }
        }
    });
}

DYN_COMMAND_FN(n, module) {
    module.command = "joinban";
    module.description = "Block user from joining";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = COMMAND_HANDLER_NAME(joinban);
    return true;
}