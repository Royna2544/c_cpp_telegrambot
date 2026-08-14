
#include <absl/strings/str_split.h>
#include <tgbot/tools/StringTools.h>

#include <TryParseStr.hpp>
#include <algorithm>
#include <api/CommandModule.hpp>
#include <api/StringResLoader.hpp>
#include <api/TgBotApi.hpp>
#include <functional>

#include "api/MessageExt.hpp"

constexpr int MAX_SPAM_COUNT = 10;
constexpr auto kSpamDelayTime = std::chrono::milliseconds(700);

namespace {

void try_parse_spamcnt(const std::string_view data, int* count) {
    if (try_parse(data, count)) {
        *count = std::clamp(*count, 0, MAX_SPAM_COUNT);
    } else {
        LOG(WARNING) << "Failed to parse " << std::quoted(data)
                     << " as int; defaults to 1";
        *count = 1;
    }
}
}  // namespace

/**
 * @brief A command module for spamming.
 */
DECLARE_COMMAND_HANDLER(spam) {
    std::function<void(void)> fp;
    int count = 0;
    bool spamable = false;

    if (message->reply()->exists()) {
        const ChatId chatid = message->reply()->get<MessageAttrs::Chat>()->id;

        spamable = true;
        try_parse_spamcnt(message->get<MessageAttrs::ExtraText>(), &count);
        if (message->reply()->has<MessageAttrs::Sticker>()) {
            const MediaIds sticker(
                message->reply()->get<MessageAttrs::Sticker>());
            fp = [api, chatid, sticker] { api->sendSticker(chatid, sticker); };
        } else if (message->reply()->has<MessageAttrs::Animation>()) {
            const MediaIds animation(
                message->reply()->get<MessageAttrs::Animation>());
            fp = [api, chatid, animation] {
                api->sendAnimation(chatid, animation);
            };
        } else if (message->reply()->has<MessageAttrs::ExtraText>()) {
            const auto text = message->reply()->get<MessageAttrs::ExtraText>();
            fp = [api, chatid, text] { api->sendMessage(chatid, text); };
        } else {
            api->sendReplyMessage(message->message(),
                                  res->get(Strings::SPAM_REPLY_SUPPORTS));
            spamable = false;
        }

    } else if (message->has<MessageAttrs::ExtraText>()) {
        std::vector<std::string> commands;
        std::pair<int, std::string> spamData;
        commands = absl::StrSplit(message->get<MessageAttrs::ExtraText>(), ' ',
                                  absl::SkipWhitespace());
        if (commands.size() == 2) {
            try_parse_spamcnt(commands[0], &spamData.first);
            spamData.second = commands[1];
            const ChatId chatid = message->get<MessageAttrs::Chat>()->id;
            fp = [api, chatid, spamData] {
                api->sendMessage(chatid, spamData.second);
            };
            count = spamData.first;
            spamable = true;
        } else {
            api->sendReplyMessage(message->message(),
                                  res->get(Strings::SPAM_INVALID_CONFIG_SIZE));
        }
    } else {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::SPAM_SEND_CONFIG));
    }
    if (spamable) {
        for (int index = 0; index < count; ++index) {
            if (!api->submitCommandWork(
                    "spam", TgBotApi::WorkClass::Outbound,
                    [fp](std::stop_token stop) {
                        if (!stop.stop_requested())
                            fp();
                    },
                    {.delay = kSpamDelayTime * index,
                     .deadline = std::chrono::seconds(30)})) {
                LOG(WARNING) << "Spam outbound queue rejected item " << index;
                break;
            }
        }
    }
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::OwnerOnly,
    .name = "spam",
    .description = "Spam a given literal or media",
    .function = COMMAND_HANDLER_NAME(spam),
    .valid_args =
        {
            .enabled = true,
            .counts = DynModule::craftArgCountMask<1, 2>(),
        },
};
