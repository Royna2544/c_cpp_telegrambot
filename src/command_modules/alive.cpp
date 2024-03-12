#include <Logging.h>
#include <ResourceIncBin.h>
#include <cmds.gen.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/config.hpp>
#include <map>
#include <mutex>
#include <popen_wdt/popen_wdt.hpp>
#include <string>

#include "CommandModule.h"
#include "internal/_tgbot.h"

static void AliveCommandFn(const Bot &bot, const Message::Ptr message) {
    static std::string version;
    static std::once_flag once;

    std::call_once(once, [&bot] {
        std::string commitid, commitmsg, originurl, compilerver, commandmodules;

        static const std::map<std::string *, std::string> commands = {
            {&commitid, "rev-parse HEAD"},
            {&commitmsg, "log --pretty=%s -1"},
            {&originurl, "config --get remote.origin.url"},
        };
        for (const auto &cmd : commands) {
            const std::string gitPrefix =
                "git --git-dir=" + (getSrcRoot() / ".git").string() + ' ';
            const bool ret = runCommand(gitPrefix + cmd.second, *cmd.first);
            if (!ret) {
                LOG_E("Command failed: %s", cmd.second.c_str());
                *cmd.first = "[Failed]";
            }
        }
        compilerver =
            std::string(BOOST_PLATFORM " | " BOOST_COMPILER " | " __DATE__);
        commandmodules.reserve(8 * gCmdModules.size());
        for (const auto &i : gCmdModules) {
            commandmodules += i->command;
            commandmodules += " ";
        }
        ASSIGN_INCTXT_DATA(AboutHtmlText, version);
#define REPLACE_PLACEHOLDER(buf, name) \
    boost::replace_all(buf, "_" #name "_", name)
#define REPLACE_PLACEHOLDER2(buf, name, val) \
    boost::replace_all(buf, "_" #name "_", val)
        REPLACE_PLACEHOLDER(version, commitid);
        REPLACE_PLACEHOLDER(version, commitmsg);
        REPLACE_PLACEHOLDER(version, originurl);
        REPLACE_PLACEHOLDER(version, compilerver);
        REPLACE_PLACEHOLDER(version, commandmodules);
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
        LOG_E("Alive cmd: Error while sending GIF: %s", e.what());
        bot_sendReplyMessageHTML(bot, message, version);
    }
}

struct CommandModule cmd_alive(
    "alive", "Test the bot if alive, also prints bot information",
    CommandModule::Flags::None, AliveCommandFn);

struct CommandModule cmd_start(
    "start", "Alias for alive command",
    CommandModule::Flags::HideDescription, AliveCommandFn);