#include <ExtArgs.h>

#include <string>
#include "StringToolsExt.h"

std::string::size_type firstBlank(const Message::Ptr &msg) {
    return msg->text.find_first_of(" \n\r");
}

bool hasExtArgs(const Message::Ptr &message) {
    return firstBlank(message) != std::string::npos;
}

void parseExtArgs(const Message::Ptr &message, std::string &extraargs) {
    // Telegram ensures message does not have whitespaces beginning or ending.
    if (hasExtArgs(message)) {
        extraargs = message->text.substr(firstBlank(message));
        while (isEmptyChar(extraargs.front()))
            extraargs = extraargs.substr(1);
    }
}
