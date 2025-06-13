
#include <absl/strings/str_split.h>
#include <tgbot/tools/StringTools.h>

#include <TryParseStr.hpp>
#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>
#include <functional>
#include <thread>

#include "api/MessageExt.hpp"

constexpr int MAX_SPAM_COUNT = 10;
constexpr auto kSpamDelayTime = std::chrono::milliseconds(700);

namespace {

void for_count(int count, const std::function<void(void)>& callback) {
    if (count > MAX_SPAM_COUNT) {
        count = MAX_SPAM_COUNT;
    }
    for (int i = 0; i < count; ++i) {
        callback();
        std::this_thread::sleep_for(kSpamDelayTime);
    }
}
void try_parse_spamcnt(const std::string& data, int& count) {
    if (count > MAX_SPAM_COUNT) {
        count = MAX_SPAM_COUNT;
    }
    if (!try_parse(data, &count)) {
        LOG(WARNING) << "Failed to parse " << std::quoted(data) << " as int";
        count = 1;
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
        const auto chatid = message->reply()->get<MessageAttrs::Chat>();

        spamable = true;
        try_parse_spamcnt(message->get<MessageAttrs::ExtraText>(), count);
        if (message->reply()->has<MessageAttrs::Sticker>()) {
            fp = [api, message, chatid] {
                api->sendSticker(
                    chatid,
                    MediaIds(message->reply()->get<MessageAttrs::Sticker>()));
            };
        } else if (message->reply()->has<MessageAttrs::Animation>()) {
            fp = [api, message, chatid] {
                api->sendAnimation(
                    chatid,
                    MediaIds(message->reply()->get<MessageAttrs::Animation>()));
            };
        } else if (message->reply()->has<MessageAttrs::ExtraText>()) {
            fp = [api, message, chatid] {
                api->sendMessage(
                    chatid, message->reply()->get<MessageAttrs::ExtraText>());
            };
        } else {
            api->sendReplyMessage(message->message(),
                                  "Supports sticker/GIF/text for reply to "
                                  "messages, give count");
            spamable = false;
        }

    } else if (message->has<MessageAttrs::ExtraText>()) {
        std::vector<std::string> commands;
        std::pair<int, std::string> spamData;
        commands = absl::StrSplit(message->get<MessageAttrs::ExtraText>(), ' ',
                                  absl::SkipWhitespace());
        if (commands.size() == 2) {
            try_parse_spamcnt(commands[0], spamData.first);
            spamData.second = commands[1];
            fp = [api, message, spamData] {
                api->sendMessage(message->get<MessageAttrs::Chat>()->id,
                                 spamData.second);
            };
            count = spamData.first;
            spamable = true;
        } else {
            api->sendReplyMessage(message->message(),
                                  "Invalid argument size for spam config");
        }
    } else {
        api->sendReplyMessage(message->message(),
                              "Send a pair of spam count and message to spam");
    }
    if (spamable) {
        for_count(count, fp);
    }
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::Enforced,
    .name = "spam",
    .description = "Spam a given literal or media",
    .function = COMMAND_HANDLER_NAME(spam),
    .valid_args =
        {
            .enabled = true,
            .counts = DynModule::craftArgCountMask<1, 2>(),
        },
};
