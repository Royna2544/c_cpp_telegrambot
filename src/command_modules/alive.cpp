#include <GitData.h>
#include <ResourceManager.h>
#include <absl/log/log.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/config.hpp>
#include <filesystem>
#include <mutex>
#include <string_view>

#include "CommandModule.h"
#include "CompileTimeStringConcat.hpp"
#include "internal/_tgbot.h"

constexpr std::string_view kysGif =
    "CgACAgIAAx0CdMESqgACCZRlrfMoq_"
    "b2DL21k6ohShQzzLEh6gACsw4AAuSZWUmmR3jSJA9WxzQE";

template <unsigned Len>
consteval auto cat(const char (&strings)[Len]) {
    return StringConcat::cat("_", strings, "_");
}

template <unsigned N>
void REPLACE_PLACEHOLDER(std::string& str, std::string_view placeholder_name,
                         const std::string& data) {
    boost::replace_all(str, placeholder_name, data);
}

static void AliveCommandFn(const Bot& bot, const Message::Ptr message) {
    static std::string version;
    static std::once_flag once;

    std::call_once(once, [&bot] {
        std::string commandmodules;
        GitData data;
        std::string modules = cat("commandmodules");
        std::string commitid = cat("commitid");
        std::string commitmsg = cat("commitmsg");
        std::string originurl = cat("originurl");
        std::string botname = cat("botname");
        std::string botusername = cat("botusername");

        GitData::Fill(&data);
        version = ResourceManager::getInstance()->getResource("about.html");

        boost::replace_all(version, modules,  CommandModuleManager::getLoadedModulesString());
        boost::replace_all(version, commitid, data.commitid);
        boost::replace_all(version, commitmsg, data.commitmsg);
        boost::replace_all(version, originurl, data.originurl);
        boost::replace_all(version, botname, UserPtr_toString(bot.getApi().getMe()));
        boost::replace_all(version, botusername, bot.getApi().getMe()->username);
    });
    try {
        // Hardcoded kys GIF
        bot.getApi().sendAnimation(message->chat->id, kysGif.data(), 0, 0, 0,
                                   "", version, message->messageId, nullptr,
                                   "html");
    } catch (const TgBot::TgException& e) {
        // Fallback to HTML if no GIF
        LOG(ERROR) << "Alive cmd: Error while sending GIF: " << e.what();
        bot_sendReplyMessageHTML(bot, message, version);
    }
}

void loadcmd_alive(CommandModule &module) {
    module.command = "alive";
    module.description = "Test if a bot is alive";
    module.flags = CommandModule::Flags::None;
    module.fn = AliveCommandFn;
}

void loadcmd_start(CommandModule &module) {
    module.command = "start";
    module.description = "Alias for alive command";
    module.flags = CommandModule::Flags::HideDescription;
    module.fn = AliveCommandFn;
}