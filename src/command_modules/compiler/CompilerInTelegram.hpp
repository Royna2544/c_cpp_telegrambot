#pragma once

#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

#include <filesystem>
#include <sstream>
#include <string>
#include <utility>

using TgBot::Bot;
using TgBot::Message;

struct CompilerInTg {
    enum class ErrorType {
        // TODO
        START_COMPILER,
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
     * @brief This functions proivides a common interface for the command
     * implementations.
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
        : allowhang(_allowhang) {}
    bool allowhang;
    ~CompilerInTgForBash() override = default;
    void run(const Message::Ptr& message) override;
};

struct CompilerInTgForGeneric : CompilerInTg {
    explicit CompilerInTgForGeneric(std::filesystem::path _cmdPrefix,
                                    std::string _outfile)
        : cmdPrefix(std::move(_cmdPrefix)), outfile(std::move(_outfile)) {}
    std::filesystem::path cmdPrefix;
    std::string outfile;
    bool verifyParseWrite(const Message::Ptr& message, std::string& extraargs);
    ~CompilerInTgForGeneric() override = default;
    void run(const Message::Ptr& message) override;
};

struct CompilerInTgForCCpp : CompilerInTgForGeneric {
    using CompilerInTgForGeneric::CompilerInTgForGeneric;
    ~CompilerInTgForCCpp() override = default;
    void run(const Message::Ptr& message) override;
};

struct CompilerInTgForBashImpl : CompilerInTgForBash {
    explicit CompilerInTgForBashImpl(const bool allowhang)
        : CompilerInTgForBash(allowhang) {}
    ~CompilerInTgForBashImpl() override = default;

    void onResultReady(const Message::Ptr& who,
                       const std::string& text) override;
    void onFailed(const Message::Ptr& who, const ErrorType e) override;
};

struct CompilerInTgForGenericImpl : CompilerInTgForGeneric {
    CompilerInTgForGenericImpl(const std::filesystem::path& cmdPrefix,
                               const std::string& outfile)
        : CompilerInTgForGeneric(cmdPrefix, outfile) {}
    ~CompilerInTgForGenericImpl() override = default;

    void onResultReady(const Message::Ptr& who,
                       const std::string& text) override;
    void onFailed(const Message::Ptr& who, const ErrorType e) override;
};

struct CompilerInTgForCCppImpl : CompilerInTgForCCpp {
    CompilerInTgForCCppImpl(const std::filesystem::path& cmdPrefix,
                            const std::string& outfile)
        : CompilerInTgForCCpp(cmdPrefix, outfile) {}
    ~CompilerInTgForCCppImpl() override = default;

    void onResultReady(const Message::Ptr& who,
                       const std::string& text) override;
    void onFailed(const Message::Ptr& who, const ErrorType e) override;
};

struct CompilerInTgHelper {
    static void onFailed(const Message::Ptr& message,
                         const CompilerInTg::ErrorType e);
    static void onResultReady(const Message::Ptr& message,
                              const std::string& text);
    static void onCompilerPathCommand(const Message::Ptr& message,
                                      const std::string& text);
};

// Read buffer size, max allowed buffer size
constexpr const static inline auto BASH_READ_BUF = (1 << 8);
constexpr const static inline auto BASH_MAX_BUF = (1 << 10) * 3;

enum class ProLangs {
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
bool findCompiler(ProLangs lang, std::filesystem::path& path);