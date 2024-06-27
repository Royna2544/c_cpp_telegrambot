#include <BotReplyMessage.h>
#include <CStringLifetime.h>
#include <absl/log/log.h>
#include <internal/_tgbot.h>
#include <MessageWrapper.hpp>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <StringResManager.hpp>

#include <TryParseStr.hpp>
#include <sstream>

#include "CommandModule.h"

static void CloneCommandFn(const Bot& bot, const Message::Ptr message) {
    UserId uid = 0;
    ChatId cid = message->chat->id;
    bool ok = false;
    static TgBot::User::Ptr botUser = bot.getApi().getMe();
    MessageWrapper wrapper(bot, message);

    if (wrapper.hasExtraText()) {
        if (!try_parse(wrapper.getExtraText(), &uid)) {
            wrapper.sendMessageOnExit(GETSTR(FAILED_TO_PARSE_INPUT));
        }
    } else if (message->replyToMessage) {
        uid = message->replyToMessage->from->id;
        ok = true;
    } else {
        wrapper.sendMessageOnExit(GETSTR(NEED_USER));
    }
    if (ok) {
        auto member = bot.getApi().getChatMember(cid, uid);
        if (member && member->user) {
            auto user = member->user;
            CStringLifetime userName = UserPtr_toString(user);
            std::stringstream ss;
            ChatId ownerId = TgBotDatabaseImpl::getInstance()->getOwnerUserId();

            bot.getApi().setMyName(userName.get());

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
            ss << '@' << botUser->username;
            pmfn();
            ss << userName;
            pmfn();
            if (auto phs = bot.getApi().getUserProfilePhotos(uid)->photos;
                !phs.empty() && !phs.front().empty()) {
                ss << "/setuserpic";
                pmfn();
                ss << '@' << botUser->username;
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
