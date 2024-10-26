#include <GitData.h>
#include <ResourceManager.h>
#include <absl/log/log.h>
#include <absl/strings/str_replace.h>
#include <fmt/format.h>
#include <tgbot/TgException.h>
#include <trivial_helpers/_tgbot.h>

#include <CompileTimeStringConcat.hpp>
#include <api/CommandModule.hpp>
#include <api/Providers.hpp>
#include <api/TgBotApi.hpp>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <mutex>

#include "StringResLoader.hpp"

template <unsigned Len>
consteval auto cat(const char (&strings)[Len]) {
    return StringConcat::cat("_", strings, "_");
}

static DECLARE_COMMAND_HANDLER(alive) {
    static std::string version;
    static std::once_flag once;

    std::call_once(once, [provider, api] {
        std::string _version;
        GitData data;

        const auto modules = cat("commandmodules");
        const auto commitid = cat("commitid");
        const auto commitmsg = cat("commitmsg");
        const auto originurl = cat("originurl");
        const auto botname = cat("botname");
        const auto botusername = cat("botusername");

        GitData::Fill(&data);
        _version = provider->resource->get("about.html");

        // Replace placeholders in the version string with actual values.
        version = absl::StrReplaceAll(
            _version, {{commitid, data.commitid},
                       {commitmsg, data.commitmsg},
                       {botname, api->getBotUser()->firstName},
                       {botusername, api->getBotUser()->username}});
    });
    const auto info = provider->database->queryMediaInfo("alive");
    bool sentAnimation = false;
    if (info && info->mediaType == DatabaseBase::MediaType::GIF) {
        try {
            api->sendReplyAnimation<TgBotApi::ParseMode::HTML>(
                message->message(),
                MediaIds{info->mediaId, info->mediaUniqueId}, version);
            sentAnimation = true;
        } catch (const TgBot::TgException& e) {
            // Fallback to HTML if no GIF
            LOG(ERROR) << fmt::format(
                "{}: {}", access(res, Strings::ERROR_SENDING_GIF), e.what());
        }
    }
    if (!sentAnimation) {
        api->sendReplyMessage<TgBotApi::ParseMode::HTML>(message->message(),
                                                         version);
    }
}

DYN_COMMAND_FN(name, module) {
    if (name != "alive" && name != "start") {
        return false;
    }
    module.flags = CommandModule::Flags::None;
    module.name = name;
    module.description = "Test if a bot is alive";
    module.function = COMMAND_HANDLER_NAME(alive);
    return true;
}
