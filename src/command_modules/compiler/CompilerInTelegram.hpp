#pragma once

#include <absl/status/status.h>
#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "TgBotWrapper.hpp"

using TgBot::Bot;
using TgBot::Message;

struct CompilerInTg {
    // Interface for handling events from this class
    struct Interface {
        virtual ~Interface() = default;
        virtual void onExecutionStarted(const std::string_view& command) = 0;
        virtual void onExecutionFinished(const std::string_view& command) = 0;
        virtual void onErrorStatus(absl::Status status) = 0;
        virtual void onResultReady(const std::string& text) = 0;
        virtual void onWdtTimeout() = 0;
    };

    /**
     * @brief This functions proivides a common interface for the command
     * implementations.
     */
    virtual void run(MessagePtr message) = 0;

    explicit CompilerInTg(std::shared_ptr<Interface> interface);
    virtual ~CompilerInTg() = default;

    /**
     * @brief This function is used to execute a command and return the output.
     *
     * @param cmd The command to be executed.
     * @param res The output of the command.
     * @param use_wdt Whether to use the watchdog to time out the command.
     * If set to false, the function will not time out the command.
     */
    void runCommand(std::string cmd, std::stringstream& res,
                    bool use_wdt = true);

    constexpr static const char SPACE = ' ';
    constexpr static const char EMPTY[] = "(empty)";

   protected:
    std::shared_ptr<Interface> _interface;
};

struct CompilerInTgForBash : CompilerInTg {
    using CompilerInTg::CompilerInTg;
    void allowHang(bool _allowhang) { allowhang = _allowhang; }
    ~CompilerInTgForBash() override = default;
    void run(MessagePtr message) override;

   private:
    bool allowhang;
};

struct CompilerInTgForGeneric : CompilerInTg {
    struct Params {
        // e.g. /usr/bin/clang (Before the filename)
        std::filesystem::path exe;
        // e.g. /tmp/output.c (Where to write the code to compile/interpret)
        std::filesystem::path outfile;
    };
    explicit CompilerInTgForGeneric(std::shared_ptr<Interface> interface,
                                    Params params)
        : CompilerInTg(std::move(interface)), params(std::move(params)) {}

    Params params;
    bool verifyParseWrite(const MessagePtr message, std::string& extraargs);
    ~CompilerInTgForGeneric() override = default;
    void run(MessagePtr message) override;
};

struct CompilerInTgForCCpp : CompilerInTgForGeneric {
    using CompilerInTgForGeneric::CompilerInTgForGeneric;
    using CompilerInTgForGeneric::Params;
    ~CompilerInTgForCCpp() override = default;
    void run(MessagePtr message) override;
};

// Read buffer size, max allowed buffer size
constexpr const static inline auto BASH_READ_BUF = (1 << 8);
constexpr const static inline auto BASH_MAX_BUF = (1 << 10) * 3;
