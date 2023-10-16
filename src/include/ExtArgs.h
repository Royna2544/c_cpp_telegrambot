#include "NamespaceImport.h"

static inline bool hasExtArgs(const Message::Ptr &message) {
    return message->text.find_first_of(" \n") != std::string::npos;
}

void parseExtArgs(const Message::Ptr &message, std::string &extraargs);
