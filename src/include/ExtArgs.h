#include <string>
#include <tgbot/types/Message.h>

using TgBot::Message;

bool hasExtArgs(const Message::Ptr &message);
void parseExtArgs(const Message::Ptr &message, std::string &extraargs);
