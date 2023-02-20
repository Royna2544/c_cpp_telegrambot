#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#include <tgbot/tgbot.h>

#include "Timer.h"

using namespace TgBot;

// #define DEBUG

static std::vector<int64_t> recognized_chatids = {
    -1001895513214,  // Our spamgroup
};

static bool Authorized(const Message::Ptr &message) {
	if (std::find(recognized_chatids.begin(), recognized_chatids.end(),
		      message->chat->id) == recognized_chatids.end()) {
		printf("Unknown chat: %ld\n", message->chat->id);
		return false;
	}
	return true;
}

#define FILENAME "./compile.cpp"
#define AOUTNAME "./a.out"
#define STDERRTOOUT "2>&1"
#define BUFSIZE 1024
#define EMPTY "<empty>"
#define SPACE " "
static void CCppCompileHandler(const Bot *bot, const Message::Ptr &message,
			       const bool plusplus) {
	FILE *fp;
	std::string res;
	std::stringstream cmd, cmd2;
	std::unique_ptr<char[]> buff;
	bool fine;

	if (message->replyToMessage == nullptr) {
		bot->getApi().sendMessage(message->chat->id,
					  "Reply to a code to compile", false,
					  message->messageId);
		return;
	}

	std::ofstream file;
	file.open(FILENAME);
	if (file.fail()) {
		bot->getApi().sendMessage(message->chat->id,
					  "Failed to open file to compile",
					  false, message->messageId);
		return;
	}
	file << message->replyToMessage->text;
	file.close();

	if (plusplus) {
		cmd << "c++";
	} else {
		cmd << "cc";
	}
	cmd << SPACE << "-x" << SPACE;
	if (plusplus) {
		cmd << "c++";
	} else {
		cmd << "c";
	}
	cmd << SPACE << FILENAME << SPACE << STDERRTOOUT;
#ifdef DEBUG
	printf("cmd: %s\n", cmd.str().c_str());
#endif
	fp = popen(cmd.str().c_str(), "r");
	if (!fp) {
		bot->getApi().sendMessage(message->chat->id,
					  "Failed to popen()", false,
					  message->messageId);
		return;
	}
	buff = std::make_unique<char[]>(BUFSIZE);
	res += "Compile time:\n";
	// fine is implicit false here
	while (fgets(buff.get(), BUFSIZE, fp)) {
		if (!fine) fine = true;
		res += buff.get();
	}
	pclose(fp);
	if (!fine) res += EMPTY;

	res += "\n";

	std::ifstream aout(AOUTNAME);
	if (!aout.good()) goto sendresult;
	cmd.swap(cmd2);
	cmd << AOUTNAME << SPACE << STDERRTOOUT;
	fp = popen(cmd.str().c_str(), "r");
	res += "Run time:\n";
	fine = false;
	buff = std::make_unique<char[]>(BUFSIZE);
	while (fgets(buff.get(), BUFSIZE, fp)) {
		if (!fine) fine = true;
		res += buff.get();
	}
	if (!fine) res += EMPTY;
	pclose(fp);

sendresult:
	bot->getApi().sendMessage(message->chat->id, res.c_str(), false,
				  message->messageId);
	std::remove(FILENAME);
	std::remove(AOUTNAME);
}
int main(void) {
	const char *token_str = getenv("TOKEN");
	if (!token_str) {
		printf("TOKEN is not exported\n");
		return 1;
	}
	std::string token(token_str);
	printf("Token: %s\n", token.c_str());

	Bot bot(token);
	static Timer *tm_ptr;
	bot.getEvents().onCommand("cpp", [&bot](Message::Ptr message) {
		if (!Authorized(message)) return;
		CCppCompileHandler(&bot, message, 1);
	});
	bot.getEvents().onCommand("c", [&bot](Message::Ptr message) {
		if (!Authorized(message)) return;
		CCppCompileHandler(&bot, message, 0);
	});
	bot.getEvents().onCommand("alive", [&bot](Message::Ptr message) {
		if (!Authorized(message)) return;
		bot.getApi().sendMessage(message->chat->id, "I am alive...",
					 false, message->messageId);
	});
	bot.getEvents().onCommand("flash", [&bot](Message::Ptr message) {
		if (!Authorized(message)) return;
		static std::vector<std::string> reasons = {
		    "Alex is not sleeping",
		    "The system has been destoryed",
		};

		std::string msg = message->text;
		if (msg.find(" ") == std::string::npos) {
			bot.getApi().sendMessage(message->chat->id,
						 "Send a file name", false,
						 message->messageId);
			return;
		}
		msg = msg.substr(msg.find(" ") + 1);
		std::replace(msg.begin(), msg.end(), ' ', '_');
		std::stringstream ss;
		ss << "Flashing '" << msg;
		ss << ".zip' failed!" << std::endl;
		ss << "Reason: ";
		srand(time(0));
		ssize_t pos = rand() % reasons.size();
		ss << reasons[pos];
		bot.getApi().sendMessage(message->chat->id, ss.str());
	});
	bot.getEvents().onCommand("shutdown", [&bot](Message::Ptr message) {
		if (message->from->id == 1185607882 && std::time(0) - message->date < 5) exit(0);
	});
	bot.getEvents().onCommand("starttimer", [&bot](Message::Ptr message) {
		enum InputState {
			HOUR,
			MINUTE,
			SECOND,
			NONE,
		};
		if (!Authorized(message)) return;
		if (message->replyToMessage == nullptr) {
			bot.getApi().sendMessage(
			    message->chat->id,
			    "Reply to a time, in hhmmss format", false,
			    message->messageId);
			return;
		}
		if (tm_ptr && tm_ptr->isrunning()) {
			bot.getApi().sendMessage(message->chat->id,
						 "Timer is already running",
						 false, message->messageId);
			return;
		}
		const char *c_str = message->replyToMessage->text.c_str();
		std::vector<int> numbercache;
		enum InputState state;
		struct timehms hms = {0};
		for (int i = 0; i <= message->replyToMessage->text.size();
		     i++) {
			int code = static_cast<int>(c_str[i]);
			if (i == message->replyToMessage->text.size())
				code = 32;
			switch (code) {
				case ' ': {
					if (numbercache.size() != 0) {
						int result = 0, count = 1;
						for (const auto i :
						     numbercache) {
							result +=
							    pow(10,
								numbercache
									.size() -
								    count) *
							    i;
							count++;
						}
						switch (state) {
							case InputState::HOUR:
								hms.h = result;
								break;
							case InputState::MINUTE:
								hms.m = result;
								break;
							case InputState::SECOND:
								hms.s = result;
								break;
							default:
								break;
						}
#ifdef DEBUG
						printf("result: %d\n", result);
#endif
						state = InputState::NONE;
						numbercache.clear();
					}
					break;
				}
				case '0' ... '9': {
					int intver = code - 48;
#ifdef DEBUG
					printf("%d\n", intver);
#endif
					numbercache.push_back(intver);
					break;
				}
				case 'H':
				case 'h': {
					state = InputState::HOUR;
					break;
				}
				case 'M':
				case 'm': {
					state = InputState::MINUTE;
					break;
				}
				case 'S':
				case 's': {
					state = InputState::SECOND;
					break;
				}
			}
		}
#ifdef DEBUG
		printf("Date h %d m %d s %d\n", hms.h, hms.m, hms.s);
#endif
#define TIMER_CONFIG_SEC 4
		if (!hms.h && !hms.m && !hms.s) {
			bot.getApi().sendMessage(message->chat->id,
						 "I'm not a fool to time 0s",
						 false, message->messageId);
			return;
		} else if (hms.toSeconds() < TIMER_CONFIG_SEC) {
			bot.getApi().sendMessage(message->chat->id,
						 "Provide longer time value",
						 false, message->messageId);
			return;
		}
		int msgid = bot.getApi()
				.sendMessage(message->chat->id, "Timer starts")
				->messageId;
		try {
			bot.getApi().pinChatMessage(message->chat->id, msgid);
		} catch (const std::exception &) {
			printf("Cannot pin msg!\n");
		}
		tm_ptr = new Timer(hms.h, hms.m, hms.s);
		tm_ptr->setCallback(
		    [=](void *priv, struct timehms ms) {
			    Bot *bott = reinterpret_cast<decltype(bott)>(priv);
			    std::stringstream ss;
			    if (ms.h != 0) ss << ms.h << "h ";
			    if (ms.m != 0) ss << ms.m << "m ";
			    if (ms.s != 0) ss << ms.s << "s ";
			    if (!ss.str().empty() && ss.str() != message->text)
				    bott->getApi().editMessageText(
					ss.str(), message->chat->id, msgid);
		    },
		    TIMER_CONFIG_SEC,
		    [=](void *priv) {
			    Bot *bott = reinterpret_cast<decltype(bott)>(priv);
			    bott->getApi().editMessageText(
				"Timer ended", message->chat->id, msgid);
			    std::this_thread::sleep_for(
				std::chrono::seconds(3));
			    bott->getApi().unpinChatMessage(message->chat->id,
							    msgid);
		    },
		    &bot);
		tm_ptr->start();
	});
	bot.getEvents().onCommand("stoptimer", [&bot](Message::Ptr message) {
		if (tm_ptr) tm_ptr->cancel();
		bot.getApi().sendMessage(
		    message->chat->id,
		    tm_ptr ? "Stopped successfully" : "Timer is not running",
		    false, message->messageId);
	});
	bot.getEvents().onAnyMessage([&bot](Message::Ptr message) {
		static std::vector<Message::Ptr> buffer;
		static std::mutex m;
		static std::atomic_bool cb;
		static bool falseth;

		if (std::time(0) - message->date > 10) return;
		if (!falseth) {
			std::thread([cb = &cb]() {
				falseth = true;
				while (true) {
					if (!cb) break;
					cb->store(false);
					std::this_thread::sleep_for(
					    std::chrono::seconds(5));
				}
				falseth = false;
			}).detach();
		}

		if (!Authorized(message)) return;
		{
			const std::lock_guard<std::mutex> _(m);
			buffer.push_back(message);
		}
		if (!cb) {
			cb.store(true);
			std::thread([&]() {
				struct simpleuser {
					int64_t id;
					std::string username;
					int spamcnt;
				};
				bool spam = false;
				std::vector<struct simpleuser> spamvec;
				std::this_thread::sleep_for(
				    std::chrono::seconds(5));
				const std::lock_guard<std::mutex> _(m);
#ifdef DEBUG
				printf("Buffer size: %lu\n", buffer.size());
#endif
				if (buffer.size() >= 4) {
					int64_t chatid =
					    buffer.front()->chat->id;
					for (const auto &msg : buffer) {
						if (msg->chat->id != chatid)
							continue;
						bool found = false;
						struct simpleuser *ptr;
						for (const auto &user :
						     spamvec) {
							found = user.id ==
								msg->from->id;
							if (found) {
								ptr = const_cast<
								    struct
								    simpleuser
									*>(
								    &user);
								break;
							}
						}
						if (found) {
							ptr->spamcnt += 1;
						} else {
							spamvec.push_back(
							    {msg->from->id,
							     msg->from
								 ->username,
							     1});
						}
					}
					spam = spamvec.size() < 3;
				}
				if (spam) {
					using pair_type =
					    decltype(spamvec)::value_type;
					auto pr = std::max_element(
					    std::begin(spamvec),
					    std::end(spamvec),
					    [](const pair_type &p1,
					       const pair_type &p2) {
						    return p1.spamcnt <
							   p2.spamcnt;
					    });
					bot.getApi().sendMessage(
					    buffer.front()->chat->id,
					    "Spam detected @" + pr->username);
					try {
						for (const auto &msg : buffer) {
							if (msg->from->id !=
							    pr->id)
								continue;
#ifdef DEBUG
							// Can get 'Too many
							// requests' here
							bot.getApi()
							    .sendMessage(
								buffer.front()
								    ->chat->id,
								"Delete '" +
								    msg->text +
								    "'");
#endif
							bot.getApi()
							    .deleteMessage(
								msg->chat->id,
								msg->messageId);
						}
					} catch (std::exception &) {
						printf("Error deleting msg\n");
					}
				}
				buffer.clear();
			}).detach();
		}
	});
	signal(SIGINT, [](int s) {
		printf("\n");
		printf("Got SIGINT\n");
		exit(0);
	});

	try {
		printf("Bot username: %s\n",
		       bot.getApi().getMe()->username.c_str());
		bot.getApi().deleteWebhook();

		TgLongPoll longPoll(bot);
		while (true) {
			longPoll.start();
		}
	} catch (std::exception &e) {
		printf("error: %s\n", e.what());
	}
}
