#include <fmt/core.h>

#include <Random.hpp>
#include <TgBotWrapper.hpp>
#include <sstream>
#include <thread>

using std::chrono_literals::operator""s;

DECLARE_COMMAND_HANDLER(decide, wrapperBot, message) {
    constexpr int COUNT_MAX = 10;
    constexpr int RANDOM_RANGE_NUM = 10;

    std::string obj = message->get<MessageExt::Attrs::ExtraText>();
    std::stringstream msgtxt;
    Message::Ptr msg;
    int count = COUNT_MAX;
    int yesno = 0;

    msgtxt << fmt::format("Deciding '{}'...", obj);
    msg = wrapperBot->sendReplyMessage(message, msgtxt.str());
    msgtxt << std::endl << std::endl;
    do {
        msgtxt << fmt::format("Try {}: ", COUNT_MAX - count + 1);
        if (Random::getInstance()->generate(RANDOM_RANGE_NUM) % 2 == 1) {
            msgtxt << "Yes";
            ++yesno;
        } else {
            msgtxt << "No";
            --yesno;
        }
        msgtxt << std::endl;
        count--;
        wrapperBot->editMessage(msg, msgtxt.str());
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
    wrapperBot->editMessage(msg, msgtxt.str());
}

DYN_COMMAND_FN(/*name*/, module) {
    module.command = "decide";
    module.description = "Decide a statement";
    module.flags = CommandModule::Flags::None;
    module.function = COMMAND_HANDLER_NAME(decide);
    module.valid_arguments.enabled = true;
    module.valid_arguments.counts.emplace_back(1);
    module.valid_arguments.split_type = CommandModule::ValidArgs::Split::None;
    module.valid_arguments.usage = "/decide <statement>";
    return true;
}
