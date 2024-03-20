#include <GitData.h>
#include <Logging.h>
#include <ResourceManager.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/config.hpp>
#include <filesystem>
#include <mutex>
#include <string>

#include "CommandModule.h"
#include "internal/_tgbot.h"

static void AliveCommandFn(const Bot &bot, const Message::Ptr message) {
    static std::string version;
    static std::once_flag once;

    std::call_once(once, [&bot] {
        std::string commandmodules;
        GitData data;

        GitData::Fill(&data);
        commandmodules = CommandModule::getLoadedModulesString();
        version = gResourceManager.getResource("about.html.txt");

#define REPLACE_PLACEHOLDER(buf, name) \
    boost::replace_all(buf, "_" #name "_", name)
#define REPLACE_PLACEHOLDER2(buf, name, val) \
    boost::replace_all(buf, "_" #name "_", val)
        REPLACE_PLACEHOLDER(version, commandmodules);
        REPLACE_PLACEHOLDER2(version, compilerver,
                             BOOST_PLATFORM " | " BOOST_COMPILER
                                            " | " __DATE__);
        REPLACE_PLACEHOLDER2(version, commitid, data.commitid);
        REPLACE_PLACEHOLDER2(version, commitmsg, data.commitmsg);
        REPLACE_PLACEHOLDER2(version, originurl, data.originurl);
        REPLACE_PLACEHOLDER2(version, botname,
                             UserPtr_toString(bot.getApi().getMe()));
        REPLACE_PLACEHOLDER2(version, botusername,
                             bot.getApi().getMe()->username);
    });
    try {
        // Hardcoded kys GIF
        bot.getApi().sendAnimation(
            message->chat->id,
            "CgACAgIAAx0CdMESqgACCZRlrfMoq_"
            "b2DL21k6ohShQzzLEh6gACsw4AAuSZWUmmR3jSJA9WxzQE",
            0, 0, 0, "", version, message->messageId, nullptr, "html");
    } catch (const TgBot::TgException &e) {
        // Fallback to HTML if no GIF
        LOG(LogLevel::ERROR, "Alive cmd: Error while sending GIF: %s",
            e.what());
        bot_sendReplyMessageHTML(bot, message, version);
    }
}

struct CommandModule cmd_alive("alive", "Test the bot if alive",
                               CommandModule::Flags::None, AliveCommandFn);

struct CommandModule cmd_start("start", "Alias for alive command",
                               CommandModule::Flags::HideDescription,
                               AliveCommandFn);