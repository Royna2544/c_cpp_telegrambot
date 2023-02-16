#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <string>
#include <vector>

#include <tgbot/tgbot.h>

#define DEBUG

using namespace TgBot;

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
static void CCppCompileHandler(const Bot *bot, const Message::Ptr &message, const bool plusplus) {
	FILE *fp;
	std::string cmd, res;
	char* buff;
	bool fine = true;

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

	if (plusplus) { cmd += "g++"; } else { cmd += "gcc"; }
	cmd += " ";
	cmd += FILENAME;
	cmd += " ";
	cmd += "-x";
	cmd += " ";
	if (plusplus) { cmd += "c++"; } else { cmd += "c"; }
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
	buff = new char[BUFSIZE];
	res += "Compile time:\n";
	while(fgets(buff, BUFSIZE, fp))
		res += buff;
	pclose(fp);
	delete buff;

	std::ifstream aout(AOUTNAME);
	if (!aout.good()) goto sendresult;
	cmd = AOUTNAME;
	cmd += " ";
	cmd += STDERRTOOUT;
	fp = popen(cmd.c_str(), "r");
	buff = new char[BUFSIZE];
	res += "Run time:\n";
	while(fgets(buff, BUFSIZE, fp))
		res += buff;
	pclose(fp);
	delete buff;

sendresult:
	bot->getApi().sendMessage(message->chat->id, res.c_str(), false, message->messageId);
	std::remove(FILENAME);
	std::remove(AOUTNAME);
}

int main(void) {
	std::string token(getenv("TOKEN"));
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
	/*
	bot.getEvents().onAnyMessage([&bot](Message::Ptr message) {
		printf("User wrote %s\n", message->text.c_str());
		if (StringTools::startsWith(message->text, "/start")) {
			return;
		}
		bot.getApi().sendMessage(message->chat->id, "Your message is: " + message->text);
	});
	*/
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
			printf("Long poll started\n");
			longPoll.start();
		}
	} catch (std::exception& e) {
		printf("error: %s\n", e.what());
	}
}
