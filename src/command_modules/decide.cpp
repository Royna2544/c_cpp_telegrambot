#include <ExtArgs.h>
#include <Logging.h>
#include <StringToolsExt.h>
#include <random/RandomNumberGenerator.h>

#include "CommandModule.h"

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
            std_sleep_s(2);
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
    }
}

struct CommandModule cmd_decide {
    .enforced = false,
    .name = "decide",
    .fn = DecideCommandFn,
};