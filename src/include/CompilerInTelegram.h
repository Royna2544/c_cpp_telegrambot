#pragma once

#include "NamespaceImport.h"

struct HandleData {
    Bot &bot;
    const Message::Ptr &message;
};

struct BashHandleData : HandleData {};

struct CompileHandleData : HandleData {
    const char *cmdPrefix, *outfile;
};

struct CCppCompileHandleData : CompileHandleData {};

template <typename T = CompileHandleData,
          std::enable_if_t<std::is_base_of<HandleData, T>::value, bool> = true>
void CompileRunHandler(const T &data);

static inline bool hasExtArgs(const Message::Ptr &message) {
    return message->text.find_first_of(" \n") != std::string::npos;
}

void parseExtArgs(const Message::Ptr &message, std::string &extraargs);
