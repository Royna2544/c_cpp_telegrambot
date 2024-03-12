#include <ExtArgs.h>
#include <Logging.h>
#include <StringToolsExt.h>
#include <random/RandomNumberGenerator.h>

#include <thread>

#include "BotReplyMessage.h"
#include "CommandModule.h"

using std::chrono_literals::operator""s;

static void DecideCommandFn(const Bot &bot, const Message::Ptr message) {
    constexpr int COUNT_MAX = 10;

    if (hasExtArgs(message)) {
        std::string obj, msgtxt;
        Message::Ptr msg;
        int count = COUNT_MAX, yesno = 0;

        parseExtArgs(message, obj);
        msgtxt = "Deciding '" + obj + "'...";
        msg = bot_sendReplyMessage(bot, message, msgtxt);
        msgtxt += "\n\n";
        do {
            msgtxt += "Try " + std::to_string(COUNT_MAX - count + 1) + " : ";
            if (genRandomNumber(10) % 2 == 1) {
                msgtxt += "Yes";
                ++yesno;
            } else {
                msgtxt += "No";
                --yesno;
            }
            msgtxt += '\n';
            count--;
            bot_editMessage(bot, msg, msgtxt);
            if (count != 0) {
                if (abs(yesno) > count) {
                    msgtxt += "Short circuited to the answer\n";
                    break;
                }
            } else  // count == 0
                break;
            std::this_thread::sleep_for(2s);
        } while (count > 0);
        msgtxt += '\n';
        switch (yesno) {
            case 1 ... COUNT_MAX: {
                msgtxt += "So, yes.";
                break;
            }
            case 0: {
                msgtxt += "So, idk.";
                break;
            }
            case -COUNT_MAX... - 1: {
                msgtxt += "So, no.";
                break;
            }
        }
        bot_editMessage(bot, msg, msgtxt);
    } else {
        bot_sendReplyMessage(bot, message, "What should I decide?");
    }
}

struct CommandModule cmd_decide("decide", "Decide a statement",
                                CommandModule::Flags::None, DecideCommandFn);