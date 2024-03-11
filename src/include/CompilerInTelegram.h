#pragma once

#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

#include <string>
#include <sstream>

using TgBot::Bot;
using TgBot::Message;

struct CompileHandleData;
struct HandleData {
    const Message::Ptr message;
    enum class ErrorType {
        MESSAGE_VERIFICATION_FAILED,
        FILE_WRITE_FAILED,
        POPEN_WDT_FAILED,
    };
    virtual void onFailed(const ErrorType e) = 0;
    virtual void onResultReady(const std::string &text) = 0;
    virtual void run() = 0;
    explicit HandleData(const Message::Ptr _message) : message(_message) {}

    bool verifyMessage();
    bool writeMessageToFile(const std::string &filename);
    void appendExtArgs(std::stringstream &cmd, std::string &extraargs, std::string &res);
    void runCommand(std::string cmd, std::string &res, bool use_wdt = true);
    void commonCleanup(const std::string &res, const std::string &filename = "");
    constexpr static const char SPACE = ' ';
    constexpr static const char EMPTY[] = "(empty)";
};

struct HandleDataImpl : HandleData {
    explicit HandleDataImpl(const Bot &bot, const Message::Ptr _message) : HandleData(_message), _bot(bot) {}
    void onFailed(const ErrorType e) override;
    void onResultReady(const std::string &text) override;

   protected:
    const Bot &_bot;
};

struct BashHandleData : HandleDataImpl {
    explicit BashHandleData(const Bot &bot, const Message::Ptr message,
                            const bool _allowhang) : HandleDataImpl(bot, message), allowhang(_allowhang) {}
    bool allowhang;
    void run() override;
};

struct CompileHandleData : HandleDataImpl {
    explicit CompileHandleData(const Bot &bot, const Message::Ptr message,
                               const std::string& _cmdPrefix, const std::string& _outfile)
                               : HandleDataImpl(bot, message), cmdPrefix(_cmdPrefix), outfile(_outfile) {}
    std::string cmdPrefix, outfile;
    bool commonVPW(std::string &extraargs);
    void onCompilerPathCommand(const std::string &text);
    virtual void run() override;
};

struct CCppCompileHandleData : CompileHandleData {
    using CompileHandleData::CompileHandleData;
    void run() override;
};

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
