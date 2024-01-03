#pragma once

#include <string>

#include "NamespaceImport.h"

struct HandleData {
    const Bot &bot;
    const Message::Ptr &message;
};

struct BashHandleData : HandleData {
    bool allowhang;
};

struct CompileHandleData : HandleData {
    std::string cmdPrefix, outfile;
};

struct CCppCompileHandleData : CompileHandleData {};

template <typename T = CompileHandleData,
          std::enable_if_t<std::is_base_of<HandleData, T>::value, bool> = true>
void CompileRunHandler(const T &data);

// Read buffer size, max allowed buffer size
constexpr const static inline auto BASH_READ_BUF = (1 << 8);
constexpr const static inline auto BASH_MAX_BUF = (BASH_READ_BUF << 2) * 3;
