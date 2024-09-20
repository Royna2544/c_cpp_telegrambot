
#include <absl/strings/str_split.h>
#include <tgbot/tools/StringTools.h>

#include <TgBotWrapper.hpp>
#include <TryParseStr.hpp>
#include <functional>
#include <thread>

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
DECLARE_COMMAND_HANDLER(spam, bot, message) {
    std::function<void(void)> fp;
    int count = 0;
    bool spamable = false;

    if (message->has<MessageExt::Attrs::IsReplyMessage>()) {
        const auto chatid = message->replyToMessage->chat->id;

        spamable = true;
        try_parse_spamcnt(message->get<MessageExt::Attrs::ExtraText>(), count);
        if (message->replyToMessage_has<MessageExt::Attrs::Sticker>()) {
            fp = [bot, message, chatid] {
                bot->sendSticker(chatid,
                                 MediaIds(message->replyToMessage->sticker));
            };
        } else if (message->replyToMessage_has<
                       MessageExt::Attrs::Animation>()) {
            fp = [bot, message, chatid] {
                bot->sendAnimation(
                    chatid, MediaIds(message->replyToMessage->animation));
            };
        } else if (message->replyToMessage_has<
                       MessageExt::Attrs::ExtraText>()) {
            fp = [bot, message, chatid] {
                bot->sendMessage(chatid, message->replyToMessage->text);
            };
        } else {
            bot->sendReplyMessage(message,
                                  "Supports sticker/GIF/text for reply to "
                                  "messages, give count");
            spamable = false;
        }

    } else if (message->has<MessageExt::Attrs::ExtraText>()) {
        std::vector<std::string> commands;
        std::pair<int, std::string> spamData;
        commands = absl::StrSplit(message->text, ' ', absl::SkipWhitespace());
        if (commands.size() == 2) {
            try_parse_spamcnt(commands[0], spamData.first);
            spamData.second = commands[1];
            fp = [bot, message, spamData] {
                bot->sendMessage(message->chat->id, spamData.second);
            };
            count = spamData.first;
            spamable = true;
        } else {
            bot->sendReplyMessage(message,
                                  "Invalid argument size for spam config");
        }
    } else {
        bot->sendReplyMessage(message,
                              "Send a pair of spam count and message to spam");
    }
    if (spamable) {
        for_count(count, fp);
    }
}

DYN_COMMAND_FN(n, module) {
    module.command = "spam";
    module.description = "Spam a given literal or media";
    module.flags = CommandModule::Flags::Enforced;
    module.function = COMMAND_HANDLER_NAME(spam);
    return true;
}