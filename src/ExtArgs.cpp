#include <ExtArgs.h>
#include <NamespaceImport.h>

#include <string>

static inline std::string::size_type findBlankInMsg(const Message::Ptr& message) {
    return message->text.find_first_of(" \n");
}

bool hasExtArgs(const Message::Ptr &message) {
    return findBlankInMsg(message) != std::string::npos;
}

void parseExtArgs(const Message::Ptr &message, std::string &extraargs) {
    // Telegram ensures message does not have whitespaces beginning or ending.
    if (auto it = findBlankInMsg(message); it != std::string::npos) {
        extraargs = message->text.substr(it + 1);
    }
}
