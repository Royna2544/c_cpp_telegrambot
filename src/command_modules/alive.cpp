#include <GitData.h>
#include <ResourceManager.h>
#include <absl/log/log.h>
#include <absl/strings/str_replace.h>
#include <fmt/core.h>
#include <internal/_tgbot.h>

#include <CompileTimeStringConcat.hpp>
#include <StringResManager.hpp>
#include <TgBotWrapper.hpp>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <filesystem>
#include <mutex>
#include "database/DatabaseBase.hpp"

template <unsigned Len>
consteval auto cat(const char (&strings)[Len]) {
    return StringConcat::cat("_", strings, "_");
}

static DECLARE_COMMAND_HANDLER(alive, wrapper, message) {
    static std::string version;
    static std::once_flag once;

    std::call_once(once, [wrapper] {
        std::string _version;
        GitData data;

        const auto modules = cat("commandmodules");
        const auto commitid = cat("commitid");
        const auto commitmsg = cat("commitmsg");
        const auto originurl = cat("originurl");
        const auto botname = cat("botname");
        const auto botusername = cat("botusername");

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
    if (info && info->mediaType == DatabaseBase::MediaType::GIF) {
        try {
            wrapper->sendReplyAnimation<TgBotWrapper::ParseMode::HTML>(
                message, MediaIds{info->mediaId, info->mediaUniqueId}, version);
            sentAnimation = true;
        } catch (const TgBot::TgException& e) {
            // Fallback to HTML if no GIF
            LOG(ERROR) << fmt::format("{}: {}", GETSTR(ERROR_SENDING_GIF),
                                      e.what());
        }
    }
    if (!sentAnimation) {
        wrapper->sendReplyMessage<TgBotWrapper::ParseMode::HTML>(message,
                                                                 version);
    }
}

DYN_COMMAND_FN(name, module) {
    if (name != "alive" && name != "start") {
        return false;
    }
    module.flags = CommandModule::Flags::None;
    module.command = name;
    module.description = GETSTR(ALIVE_CMD_DESC);
    module.function = COMMAND_HANDLER_NAME(alive);
    return true;
}
