#include <TgBotWrapper.hpp>

#include "Random.hpp"
#include "internal/_tgbot.h"

DECLARE_COMMAND_HANDLER(joinban, api, message) {
    MessageWrapper wrapper(api, message);
    static std::map<Random::ret_type, std::string> banKeywords;

    if (!wrapper.hasExtraText()) {
        wrapper.sendMessageOnExit("Please provide a keyword to ban");
        return;
    }

    const auto ban_keyword = wrapper.getExtraText();

    // Remove the keyword from the list of keywords if exists.
    for (const auto& [key, value] : banKeywords) {
        if (value == ban_keyword) {
            wrapper.sendMessageOnExit("Keyword '" + ban_keyword +
                                      "' is already in use, removing it");
            api->unregisterCallback(key);
            banKeywords.erase(key);
            return;
        }
    }

    // Add the keyword to the list of keywords.
    const auto token = Random::getInstance()->generate(100, 200);
    api->sendMessage(
        message, "I will ban new users with name containing: " + ban_keyword);
    api->registerCallback(
        [ban_keyword](ApiPtr api, MessagePtr message) {
            if (const auto& newUsers = message->newChatMembers;
                !newUsers.empty()) {
                for (const auto& user : newUsers) {
                    if (UserPtr_toString(user).find(ban_keyword) !=
                        std::string::npos) {
                        api->sendMessage(message,
                                         "User banned: @" + user->username +
                                             " (keyword: " + ban_keyword + ")");
                        api->banChatMember(message->chat, user);
                        break;
                    }
                }
            }
        },
        token);
    banKeywords[token] = ban_keyword;
}

DYN_COMMAND_FN(n, module) {
    module.command = "joinban";
    module.description = "Block user from joining";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = COMMAND_HANDLER_NAME(joinban);
    return true;
}