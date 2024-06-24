#include <ExtArgs.h>
#include <random/RandomNumberGenerator.h>

#include <sstream>
#include <thread>
#include <MessageWrapper.hpp>

#include "BotReplyMessage.h"
#include "CommandModule.h"
#include "StringToolsExt.hpp"

using std::chrono_literals::operator""s;

static void DecideCommandFn(const Bot& bot, const Message::Ptr message) {
    constexpr int COUNT_MAX = 10;
    constexpr int RANDOM_RANGE_NUM = 10;

    MessageWrapper wrapper(bot, message);
    if (wrapper.hasExtraText()) {
        std::string obj = wrapper.getExtraText();
        std::stringstream msgtxt;
        Message::Ptr msg;
        int count = COUNT_MAX;
        int yesno = 0;

        msgtxt << "Deciding " << SingleQuoted(obj) << "...";
        msg = bot_sendReplyMessage(bot, message, msgtxt.str());
        msgtxt << std::endl << std::endl;
        do {
            msgtxt << "Try " + std::to_string(COUNT_MAX - count + 1) + " : ";
            if (RandomNumberGenerator::generate(RANDOM_RANGE_NUM) % 2 == 1) {
                msgtxt << "Yes";
                ++yesno;
            } else {
                msgtxt << "No";
                --yesno;
            }
            msgtxt << std::endl;
            count--;
            bot_editMessage(bot, msg, msgtxt.str());
            if (count != 0) {
                if (abs(yesno) > count) {
                    msgtxt << "Short circuited to the answer" << std::endl;
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
            msgtxt << "So, yes.";
        } else if (yesno == 0) {
            msgtxt << "So, idk.";
        } else {
            msgtxt << "So, no.";
        }
        bot_editMessage(bot, msg, msgtxt.str());
    } else {
        bot_sendReplyMessage(bot, message, "What should I decide?");
    }
}

void loadcmd_decide(CommandModule& module) {
    module.command = "decide";
    module.description = "Decide a statement";
    module.flags = CommandModule::Flags::None;
    module.fn = DecideCommandFn;
}
