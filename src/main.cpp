#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <future>
#include <chrono>
#include <mutex>
#include <atomic>
#include <memory>

#include <tgbot/tgbot.h>

using namespace TgBot;

//#define DEBUG

static std::vector<int64_t> recognized_chatids = {
	-1001895513214, // Our spamgroup
};

static bool Authorized(const Message::Ptr &message) {
	if (std::find(recognized_chatids.begin(), recognized_chatids.end(),
			message->chat->id) == recognized_chatids.end()) {
		printf("Unknown chat: %ld", message->chat->id);
		return false;
	}
	return true;
}

#define FILENAME "./compile.cpp"
#define AOUTNAME "./a.out"
#define STDERRTOOUT "2>&1"
#define BUFSIZE 1024
#define EMPTY "<empty>"
static void CCppCompileHandler(const Bot *bot, const Message::Ptr &message, const bool plusplus) {
	FILE *fp;
	std::string cmd, res;
	std::unique_ptr<char[]> buff;
	bool fine;

	if (message->replyToMessage == nullptr) {
		bot->getApi().sendMessage(message->chat->id, "Reply to a code to compile",
				false, message->messageId);
		return;
	}
	
	std::ofstream file;
	file.open(FILENAME);
	if (file.fail()) {
		bot->getApi().sendMessage(message->chat->id, "Failed to open file to compile",
				false, message->messageId);
		return;
	}
	file << message->replyToMessage->text;
	file.close();

	if (plusplus) { cmd += "c++"; } else { cmd += "cc"; }
	cmd += " ";
	cmd += "-x";
	cmd += " ";
	if (plusplus) { cmd += "c++"; } else { cmd += "c"; }
	cmd += " ";
	cmd += FILENAME;
	cmd += " ";
	cmd += STDERRTOOUT;
#ifdef DEBUG
	printf("cmd: %s\n", cmd.c_str());
#endif
	fp = popen(cmd.c_str(), "r");
	if (!fp) {
		bot->getApi().sendMessage(message->chat->id, "Failed to popen()",
			       false, message->messageId);
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
	cmd = AOUTNAME;
	cmd += " ";
	cmd += STDERRTOOUT;
	fp = popen(cmd.c_str(), "r");
	res += "Run time:\n";
	fine = false;
	buff = std::make_unique<char[]>(BUFSIZE);
	while(fgets(buff.get(), BUFSIZE, fp)) {
		if (!fine) fine = true;
		res += buff.get();
	}
	if (!fine) res += EMPTY;
	pclose(fp);

sendresult:
	bot->getApi().sendMessage(message->chat->id, res.c_str(), false, message->messageId);
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
	bot.getEvents().onCommand("cpp", [&bot](Message::Ptr message) {
		if (!Authorized(message)) return;
		CCppCompileHandler(&bot, message, 1);
	});
	bot.getEvents().onCommand("c", [&bot](Message::Ptr message) {
		if (!Authorized(message)) return;
		CCppCompileHandler(&bot, message, 0);
	});
	bot.getEvents().onAnyMessage([&bot](Message::Ptr message) {
		static std::vector<Message::Ptr> buffer;
		static std::mutex m;
		static std::atomic_bool cb;
		static bool falseth;

		if (!falseth) {
			std::thread([cb = &cb]() {
				falseth = true;
				while (true) {
					if (!cb) break;
					cb->store(false);
					std::this_thread::sleep_for(std::chrono::seconds(4));
				}
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
				std::this_thread::sleep_for(std::chrono::seconds(3));
				const std::lock_guard<std::mutex> _(m);
				if (buffer.size() > 5) {
					int64_t chatid = buffer.front()->chat->id;
					for (const auto& msg : buffer) {
						if (msg->chat->id != chatid) continue;
						bool found = false;
						struct simpleuser *ptr;
						for (const auto& user : spamvec) {
							found = user.id == msg->from->id;
							if (found) {
								ptr = const_cast<struct simpleuser *>(&user);
								break;
							}
						}
						if (found) {
							ptr->spamcnt += 1;
						} else {
							spamvec.push_back({msg->from->id, msg->from->username, 0});
						}
					}
					spam = spamvec.size() < 3;
				}
				if (spam) {
					using pair_type = decltype(spamvec)::value_type;
					auto pr = std::max_element(
						std::begin(spamvec), std::end(spamvec), 
						[] (const pair_type &p1, const pair_type &p2) {
							return p1.spamcnt < p2.spamcnt;
						}
					);
					bot.getApi().sendMessage(buffer.front()->chat->id, "Spam detected @" + pr->username);
					try {
						for (const auto &msg : buffer) {
							if (msg->from->id != pr->id) continue;
#ifdef DEBUG
							// Can get 'Too many requests' here
							bot.getApi().sendMessage(buffer.front()->chat->id, "Delete '" + msg->text + "'");
#endif
							bot.getApi().deleteMessage(msg->chat->id, msg->messageId);
						}
					} catch (std::exception&){ printf("Error deleting msg\n"); }
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
		printf("Bot username: %s\n", bot.getApi().getMe()->username.c_str());
		bot.getApi().deleteWebhook();

		TgLongPoll longPoll(bot);
		while (true) {
			longPoll.start();
		}
	} catch (std::exception& e) {
		printf("error: %s\n", e.what());
	}
}
