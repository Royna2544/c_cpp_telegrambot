#pragma once

#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#include <tgbot/tgbot.h>

struct TimerImpl_privdata {
    int32_t messageid;
    const TgBot::Bot& bot;
    bool botcanpin, sendendmsg;
    int64_t chatid;
};
