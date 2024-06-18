#include <GitData.h>
#include <ResourceManager.h>
#include <absl/log/log.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/config.hpp>
#include <filesystem>
#include <mutex>
#include <string_view>

#include "BotReplyMessage.h"
#include "CommandModule.h"
#include "CompileTimeStringConcat.hpp"
#include "database/bot/TgBotDatabaseImpl.hpp"
#include "internal/_tgbot.h"

template <unsigned Len>
consteval auto cat(const char (&strings)[Len]) {
    return StringConcat::cat("_", strings, "_");
}

static void AliveCommandFn(const Bot& bot, const Message::Ptr message) {
    static std::string version;
    static std::once_flag once;

    std::call_once(once, [&bot] {
        std::string commandmodules;
        GitData data;

        const std::string modules = cat("commandmodules");
        const std::string commitid = cat("commitid");
        const std::string commitmsg = cat("commitmsg");
        const std::string originurl = cat("originurl");
        const std::string botname = cat("botname");
        const std::string botusername = cat("botusername");

        GitData::Fill(&data);
        version = ResourceManager::getInstance()->getResource("about.html");

        boost::replace_all(version, modules,
                           CommandModuleManager::getLoadedModulesString());
        boost::replace_all(version, commitid, data.commitid);
        boost::replace_all(version, commitmsg, data.commitmsg);
        boost::replace_all(version, originurl, data.originurl);
        boost::replace_all(version, botname,
                           UserPtr_toString(bot.getApi().getMe()));
        boost::replace_all(version, botusername,
                           bot.getApi().getMe()->username);
    });
    const auto info = TgBotDatabaseImpl::getInstance()->queryMediaInfo("alive");
    bool sentAnimation = false;
    if (info) {
        try {
            bot.getApi().sendAnimation(message->chat->id, info->mediaId, 0, 0,
                                       0, "", version, message->messageId,
                                       nullptr, "html");
            sentAnimation = true;
        } catch (const TgBot::TgException& e) {
            // Fallback to HTML if no GIF
            LOG(ERROR) << "Alive cmd: Error while sending GIF: " << e.what();
        }
    }
    if (!sentAnimation) {
        bot_sendReplyMessageHTML(bot, message, version);
    }
}

void loadcmd_alive(CommandModule& module) {
    module.command = "alive";
    module.description = "Test if a bot is alive";
    module.flags = CommandModule::Flags::None;
    module.fn = AliveCommandFn;
}

void loadcmd_start(CommandModule& module) {
    module.command = "start";
    module.description = "Alias for alive command";
    module.flags = CommandModule::Flags::HideDescription;
    module.fn = AliveCommandFn;
}