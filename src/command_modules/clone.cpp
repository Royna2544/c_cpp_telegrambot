#include <BotReplyMessage.h>
#include <CStringLifetime.h>
#include <ExtArgs.h>
#include <absl/log/log.h>
#include <internal/_tgbot.h>

#include <DatabaseBot.hpp>
#include <TryParseStr.hpp>
#include <sstream>

#include "CommandModule.h"

static void CloneCommandFn(const Bot& bot, const Message::Ptr message) {
    UserId uid = 0;
    ChatId cid = message->chat->id;
    bool ok = false;
    static TgBot::User::Ptr botUser = bot.getApi().getMe();

    if (hasExtArgs(message)) {
        if (!try_parse(parseExtArgs(message), &uid)) {
            bot_sendReplyMessage(bot, message, "Invalid user id");
            return;
        }
    } else if (message->replyToMessage) {
        uid = message->replyToMessage->from->id;
        ok = true;
    } else {
        bot_sendReplyMessage(bot, message, "You must specify a user id");
    }
    if (ok) {
        auto member = bot.getApi().getChatMember(cid, uid);
        if (member && member->user) {
            auto user = member->user;
            CStringLifetime userName = UserPtr_toString(user);
            std::stringstream ss;
            ChatId ownerId = DefaultBotDatabase::getInstance()->getOwnerUserId();

            LOG(INFO) << "Clone: Dest user: " << userName.get();
            bot_sendReplyMessage(bot, message, "Cloning... (see PM)");
            auto pmfn = [&bot, &ss, ownerId]() {
                std::stringstream newSs;
                bot_sendMessage(bot, ownerId, ss.str());
                ss.swap(newSs);
            };
            ss << "Data to clone from " << userName.get();
            pmfn();
            ss << "/setname";
            pmfn();
            ss << '@' << bot.getApi().getMe()->username;
            pmfn();
            ss << userName;
            pmfn();
            if (auto phs = bot.getApi().getUserProfilePhotos(uid)->photos;
                !phs.empty() && !phs.front().empty()) {
                ss << "/setuserpic";
                pmfn();
                ss << '@' << bot.getApi().getMe()->username;
                pmfn();
                bot.getApi().sendPhoto(ownerId, phs[0][0]->fileId);
            }
        }
    }
}
 
void loadcmd_clone(CommandModule &module) {
    module.command = "clone";
    module.description = "Clones identity of a user";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = CloneCommandFn;
}
