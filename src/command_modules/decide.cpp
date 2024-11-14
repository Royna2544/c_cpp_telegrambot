#include <fmt/core.h>

#include <Random.hpp>
#include <api/CommandModule.hpp>
#include <api/Providers.hpp>
#include <api/TgBotApi.hpp>
#include <memory>
#include <sstream>
#include <thread>

#include "StringResLoader.hpp"
#include "tgbot/types/ReactionTypeEmoji.h"

using std::chrono_literals::operator""s;

DECLARE_COMMAND_HANDLER(decide) {
    constexpr int COUNT_MAX = 10;
    constexpr int RANDOM_RANGE_NUM = 10;

    std::string obj = message->get<MessageAttrs::ExtraText>();
    std::stringstream msgtxt;
    Message::Ptr msg;
    int count = COUNT_MAX;
    int yesno = 0;

    msgtxt << fmt::format("{} '{}'...", access(res, Strings::DECIDING), obj);
    msg = api->sendReplyMessage(message->message(), msgtxt.str());
    msgtxt << std::endl << std::endl;
    do {
        msgtxt << fmt::format("Try {}: ", COUNT_MAX - count + 1);
        if (provider->random->generate(RANDOM_RANGE_NUM) % 2 == 1) {
            msgtxt << access(res, Strings::YES);
            ++yesno;
        } else {
            msgtxt << access(res, Strings::NO);
            --yesno;
        }
        msgtxt << std::endl;
        count--;
        api->editMessage(msg, msgtxt.str());
        if (count != 0) {
            if (abs(yesno) > count) {
                msgtxt << access(res, Strings::SHORT_CIRCUITED_TO_THE_ANSWER)
                       << std::endl;
                break;
            }
        } else {
            // count == 0
            break;
        }
        std::this_thread::sleep_for(2s);
    } while (count > 0);
    msgtxt << std::endl;
    if (yesno > 0) {
        msgtxt << access(res, Strings::SO_YES);
        auto like = std::make_shared<TgBot::ReactionTypeEmoji>();
        like->emoji = "👍";
        api->setMessageReaction(message->message(), {like}, true);
    } else if (yesno == 0) {
        msgtxt << access(res, Strings::SO_IDK);
        auto neutral = std::make_shared<TgBot::ReactionTypeEmoji>();
        neutral->emoji = "🤷‍♂";
        api->setMessageReaction(message->message(), {neutral}, true);
    } else {
        msgtxt << access(res, Strings::SO_NO);
        auto dislike = std::make_shared<TgBot::ReactionTypeEmoji>();
        dislike->emoji = "👎";
        api->setMessageReaction(message->message(), {dislike}, true);
    }
    api->editMessage(msg, msgtxt.str());
}

extern "C" const struct DynModule DYN_COMMAND_EXPORT DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "decide",
    .description = "Decide a statement",
    .function = COMMAND_HANDLER_NAME(decide),
    .valid_args =
        {
            .enabled = true,
            .counts = DynModule::craftArgCountMask<1>(),
            .split_type = DynModule::ValidArgs::Split::None,
            .usage = "/decide <statement>",
        },
};
