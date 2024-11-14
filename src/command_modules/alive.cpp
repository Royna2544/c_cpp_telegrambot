#include <GitData.h>
#include <ResourceManager.h>
#include <absl/log/log.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/str_split.h>
#include <fmt/format.h>
#include <tgbot/TgException.h>
#include <trivial_helpers/_tgbot.h>

#include <api/CommandModule.hpp>
#include <api/Providers.hpp>
#include <api/TgBotApi.hpp>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <mutex>

#include "StringResLoader.hpp"

DECLARE_COMMAND_HANDLER(alive) {
    static std::string version;
    static std::once_flag once;

    std::call_once(once, [provider, api] {
        std::string _version;
        GitData data;

        GitData::Fill(&data);
        _version = provider->resource->get("about.html");

        std::vector<std::string> splitMsg =
            absl::StrSplit(data.commitmsg, '\n');

        // Replace placeholders in the version string with actual values.
        version = absl::StrReplaceAll(
            _version, {
                {"_commitid_", data.commitid},
                {"_commitmsg_", splitMsg.front()},
                {"_botname_", api->getBotUser()->firstName},
                {"_botusername_", api->getBotUser()->username}
            });
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

extern "C" const struct DynModule DYN_COMMAND_EXPORT DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
#ifdef cmd_alive_EXPORTS
    .name = "alive",
#endif
#ifdef cmd_start_EXPORTS
    .name = "start",
#endif
    .description = "Test if a bot is alive",
    .function = COMMAND_HANDLER_NAME(alive),
    .valid_args = {
        .enabled = true,
        .counts = DynModule::craftArgCountMask<0>()
    }
};
