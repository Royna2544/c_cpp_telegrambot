#pragma once

#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

#include <sstream>
#include <string>

#include "BotClassBase.h"

using TgBot::Bot;
using TgBot::Message;

struct CompilerInTg {
    enum class ErrorType {
        MESSAGE_VERIFICATION_FAILED,
        FILE_WRITE_FAILED,
        POPEN_WDT_FAILED,
    };

    /**
     * @brief This function is called when the compile framework fails to handle
     * a request.
     *
     * @param who The message who sent the request.
     * @param e The reason for the failure.
     */
    virtual void onFailed(const Message::Ptr& who, const ErrorType e) = 0;

    /**
     * @brief This function is called when the compile framework finishes
     * compiling the code.
     *
     * @param who The message which sent the request.
     * @param text The output of the compilation.
     */
    virtual void onResultReady(const Message::Ptr& who,
                               const std::string& text) = 0;

    /**
     * @brief This functions proivides a common interface for the command implementations.
     * 
     * @param message The message who sent the request.
     */
    virtual void run(const Message::Ptr& message) = 0;

    CompilerInTg() = default;
    virtual ~CompilerInTg() = default;

    /**
     * @brief This function is used to append extra arguments to the command.
     *
     * @param cmd The command to which the extra arguments are to be appended.
     * @param extraargs The extra arguments to be appended to the command.
     * @param res The result of the command buffer after appending the extra
     * arguments.
     */
    void appendExtArgs(std::stringstream& cmd, std::string extraargs,
                       std::stringstream& res);

    /**
     * @brief This function is used to execute a command and return the output.
     *
     * @param message The message that triggered the command.
     * @param cmd The command to be executed.
     * @param res The output of the command.
     * @param use_wdt Whether to use the watchdog to time out the command.
     * If set to false, the function will not time out the command.
     */
    void runCommand(const Message::Ptr& message, std::string cmd,
                    std::stringstream& res, bool use_wdt = true);

    constexpr static const char SPACE = ' ';
    constexpr static const char EMPTY[] = "(empty)";
};

struct CompilerInTgForBash : CompilerInTg {
    explicit CompilerInTgForBash(const bool _allowhang)
        : CompilerInTg(), allowhang(_allowhang) {}
    bool allowhang;
    virtual ~CompilerInTgForBash() = default;
    void run(const Message::Ptr& message) override;
};

struct CompilerInTgForGeneric : CompilerInTg {
    explicit CompilerInTgForGeneric(const std::string& _cmdPrefix,
                                    const std::string& _outfile)
        : CompilerInTg(), cmdPrefix(_cmdPrefix), outfile(_outfile) {}
    std::string cmdPrefix, outfile;
    bool verifyParseWrite(const Message::Ptr& message, std::string& extraargs);
    virtual ~CompilerInTgForGeneric() = default;
    virtual void run(const Message::Ptr& message) override;
};

struct CompilerInTgForCCpp : CompilerInTgForGeneric {
    using CompilerInTgForGeneric::CompilerInTgForGeneric;
    virtual ~CompilerInTgForCCpp() = default;
    void run(const Message::Ptr& message) override;
};

struct CompilerInTgForBashImpl : CompilerInTgForBash, BotClassBase {
    CompilerInTgForBashImpl(const Bot& bot, const bool allowhang)
        : CompilerInTgForBash(allowhang), BotClassBase(bot) {} 
    ~CompilerInTgForBashImpl() override = default;

    void onResultReady(const Message::Ptr& who,
                       const std::string& text) override;
    void onFailed(const Message::Ptr& who, const ErrorType e) override;
};

struct CompilerInTgForGenericImpl : CompilerInTgForGeneric, BotClassBase {
    CompilerInTgForGenericImpl(const Bot& bot, const std::string& cmdPrefix,
                               const std::string& outfile)
        : CompilerInTgForGeneric(cmdPrefix, outfile), BotClassBase(bot) {}
    ~CompilerInTgForGenericImpl() override = default;

    void onResultReady(const Message::Ptr& who,
                       const std::string& text) override;
    void onFailed(const Message::Ptr& who, const ErrorType e) override;
};

struct CompilerInTgForCCppImpl : CompilerInTgForCCpp, BotClassBase {
    CompilerInTgForCCppImpl(const Bot& bot, const std::string& cmdPrefix,
                            const std::string& outfile)
        : CompilerInTgForCCpp(cmdPrefix, outfile), BotClassBase(bot) {}
    ~CompilerInTgForCCppImpl() override = default;

    void onResultReady(const Message::Ptr& who,
                       const std::string& text) override;
    void onFailed(const Message::Ptr& who, const ErrorType e) override;
};

struct CompilerInTgHelper {
    static void onFailed(const Bot& bot, const Message::Ptr& message,
                         const CompilerInTg::ErrorType e);
    static void onResultReady(const Bot& bot, const Message::Ptr& message,
                              const std::string& text);
    static void onCompilerPathCommand(const Bot& bot,
                                      const Message::Ptr& message,
                                      const std::string& text);
};

// Read buffer size, max allowed buffer size
constexpr const static inline auto BASH_READ_BUF = (1 << 8);
constexpr const static inline auto BASH_MAX_BUF = (1 << 10) * 3;

enum class ProgrammingLangs {
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
bool findCompiler(ProgrammingLangs lang, std::string& path);

void setupCompilerInTg(Bot& bot);