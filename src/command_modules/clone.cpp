#include <BotReplyMessage.h>
#include <ExtArgs.h>

#include <sstream>

#include "CStringLifetime.h"
#include "CommandModule.h"
#include "Database.h"
#include "Logging.h"
#include "internal/_tgbot.h"

static void CloneCommandFn(const Bot& bot, const Message::Ptr message) {
    UserId uid;
    ChatId cid = message->chat->id;
    bool ok = false;
    static TgBot::User::Ptr botUser = bot.getApi().getMe();

    if (hasExtArgs(message)) {
        std::string extArgs;
        parseExtArgs(message, extArgs);
        try {
            uid = std::stoull(extArgs);
            ok = true;
        } catch (...) {
            bot_sendReplyMessage(bot, message, "Invalid user id");
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
            ChatId ownerId = database::DatabaseWrapperBotImplObj::getInstance().maybeGetOwnerId();

            LOG(LogLevel::INFO, "Clone: Dest user: %s", userName.get());
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
struct CommandModule cmd_clone("clone", "Clones identity of a user",
                               CommandModule::Flags::Enforced, CloneCommandFn);