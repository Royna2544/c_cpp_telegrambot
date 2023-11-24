#pragma once

#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

#include <chrono>
#include <thread>

using TgBot::Bot;
using TgBot::Message;

using chrono_sec = std::chrono::seconds;
using std::chrono_literals::operator""s;
using std::chrono_literals::operator""ms;

template <typename _Rep, typename _Period>
static inline void std_sleep(std::chrono::duration<_Rep, _Period> time) {
	std::this_thread::sleep_for(time);
}
static inline void std_sleep_s(int seconds) {
	std::this_thread::sleep_for(chrono_sec(seconds));
}

static inline const auto pholder1 = std::placeholders::_1;
static inline const auto pholder2 = std::placeholders::_2;
