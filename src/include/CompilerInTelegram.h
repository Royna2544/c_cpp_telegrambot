#pragma once

#include <string>
#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

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

/**
 * @brief This function is a template that can be specialized for different types of data.
 * 
 * @tparam T The type of data that the function should handle. By default, it is set to CompileHandleData, 
 * which is a struct that contains the necessary information for the Compile and Run handlers.
 * @tparam std::enable_if_t<std::is_base_of<HandleData, T>::value, bool> = true This is a SFINAE (Substitution Failure Is Not An Error)
 * check that ensures that T is a type that is derived from HandleData. This is necessary because the function needs to access
 * certain members of the data struct, such as bot and message.
 * 
 * @param data The data struct that contains the necessary information for the specific handler.
 * 
 * This function is a template because it can be specialized for different types of data. This allows the same function to be used
 * for multiple handlers, without having to write separate functions for each handler.
 * 
 * The function takes a const reference to the data struct, which allows the function to be called with a reference or a copy of the data struct.
 * This is useful because it allows the function to be called with either a reference to the data that is already available, or with a copy of the data
 * that can be modified without affecting the original data.
 * 
 * The function itself is a simple wrapper that calls the appropriate specialized function based on the type of data that is passed in.
 * This allows the template to be used without having to specify the type of data explicitly, which can be cumbersome and error-prone.
 */
template <typename T = CompileHandleData,
          std::enable_if_t<std::is_base_of<HandleData, T>::value, bool> = true>
void CompileRunHandler(const T &data);

// Read buffer size, max allowed buffer size
constexpr const static inline auto BASH_READ_BUF = (1 << 8);
constexpr const static inline auto BASH_MAX_BUF = (1 << 10) * 3;

enum ProgrammingLangs {
    C,
    CXX,
    GO,
    PYTHON,
    MAX,
};

/**
 * findCompiler - find compiler's absolute path
 *
 * @param lang ProgrammingLangs enum value to query
 * @param path Search result is stored, if found, else untouched
 * @return Whether it have found the compiler path
 */
bool findCompiler(ProgrammingLangs lang, std::string &path);
