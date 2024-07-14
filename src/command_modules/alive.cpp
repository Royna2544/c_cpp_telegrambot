#include <GitData.h>
#include <ResourceManager.h>
#include <absl/log/log.h>
#include <absl/strings/str_replace.h>
#include <internal/_tgbot.h>

#include <CompileTimeStringConcat.hpp>
#include <StringResManager.hpp>
#include <TgBotWrapper.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/config.hpp>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <filesystem>
#include <mutex>

template <unsigned Len>
consteval auto cat(const char (&strings)[Len]) {
    return StringConcat::cat("_", strings, "_");
}

static void AliveCommandFn(const TgBotWrapper* wrapper, MessagePtr message) {
    static std::string version;
    static std::once_flag once;

    std::call_once(once, [wrapper] {
        std::string _version;
        GitData data;

        const std::string modules = cat("commandmodules");
        const std::string commitid = cat("commitid");
        const std::string commitmsg = cat("commitmsg");
        const std::string originurl = cat("originurl");
        const std::string botname = cat("botname");
        const std::string botusername = cat("botusername");

        GitData::Fill(&data);
        _version = ResourceManager::getInstance()->getResource("about.html");

        // Replace placeholders in the version string with actual values.
        version = absl::StrReplaceAll(
            _version, {{modules, wrapper->getCommandModulesStr()},
                       {commitid, data.commitid},
                       {commitmsg, data.commitmsg},
                       {originurl, data.originurl},
                       {botname, UserPtr_toString(wrapper->getBotUser())},
                       {botusername, wrapper->getBotUser()->username}});
    });
    const auto info = TgBotDatabaseImpl::getInstance()->queryMediaInfo("alive");
    bool sentAnimation = false;
    if (info) {
        try {
            wrapper->sendReplyAnimation(
                message, MediaIds{info->mediaId, info->mediaUniqueId});
            sentAnimation = true;
        } catch (const TgBot::TgException& e) {
            // Fallback to HTML if no GIF
            LOG(ERROR) << GETSTR(ERROR_SENDING_GIF) << e.what();
        }
    }
    if (!sentAnimation) {
        wrapper->sendReplyMessage<TgBotWrapper::ParseMode::HTML>(message,
                                                                 version);
    }
}

DYN_COMMAND_FN(name, module) {
    if (name == nullptr) {
        return false;
    }
    std::string commandName = name;
    if (commandName != "alive" && commandName != "start") {
        return false;
    }
    module.flags = CommandModule::Flags::None;
    module.command = commandName;
    module.description = GETSTR(ALIVE_CMD_DESC);
    module.fn = AliveCommandFn;
    module.isLoaded = true;
    return true;
}

