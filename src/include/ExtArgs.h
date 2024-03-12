#include <tgbot/types/Message.h>

#include <string>

using TgBot::Message;

/**
 * Returns whether the given message contains extended arguments.
 * Extended arguments are arguments passed to a command after a forward slash
 * (/). For example, in a message with the command "/start extra_arg1
 * extra_arg2", this function would return true for the message pointer that
 * points to "/start".
 *
 * @param message the Telegram message to check
 * @return whether the message contains extended arguments
 */
bool hasExtArgs(const Message::Ptr &message);

/**
 * Parse extended arguments on a message.
 * Extended arguments are arguments passed to a command after a forward slash
 * (/). For example, in a message with the command "/start extra_arg1
 * extra_arg2", this function would update extraargs with "extra_arg1
 * extra_arg2".
 *
 * @param message the Telegram message to check
 * @param extraargs a reference to a string that will contain the extra
 * arguments, if any
 */
void parseExtArgs(const Message::Ptr &message, std::string &extraargs);