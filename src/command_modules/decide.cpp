#include <ExtArgs.h>
#include <StringToolsExt.h>
#include <random/RandomNumberGenerator.h>

#include <sstream>
#include <thread>

#include "BotReplyMessage.h"
#include "CommandModule.h"

using std::chrono_literals::operator""s;

static void DecideCommandFn(const Bot &bot, const Message::Ptr message) {
    constexpr int COUNT_MAX = 10;

    if (hasExtArgs(message)) {
        std::string obj;
        std::stringstream msgtxt;
        Message::Ptr msg;
        int count = COUNT_MAX, yesno = 0;

        parseExtArgs(message, obj);
        msgtxt << "Deciding '" + obj + "'...";
        msg = bot_sendReplyMessage(bot, message, msgtxt.str());
        msgtxt << std::endl << std::endl;
        do {
            msgtxt << "Try " + std::to_string(COUNT_MAX - count + 1) + " : ";
            if (genRandomNumber(10) % 2 == 1) {
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
                    msgtxt << "Short circuited to the answer\n";
                    break;
                }
            } else  // count == 0
                break;
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

struct CommandModule cmd_decide("decide", "Decide a statement",
                                CommandModule::Flags::None, DecideCommandFn);