#include <Authorization.h>
#include <BotReplyMessage.h>
#include <CompilerInTelegram.h>
#include <NamespaceImport.h>
#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <sstream>

#include "popen_wdt/popen_wdt.h"

static const char SPACE = ' ';
static const char EMPTY[] = "(empty)";

static bool verifyMessage(const Bot &bot, const Message::Ptr &message) {
    ENFORCE_AUTHORIZED false;
    if (message->replyToMessage && !message->replyToMessage->text.empty()) {
        return true;
    }
    bot_sendReplyMessage(bot, message, "Reply to a code to compile");
    return false;
}

void parseExtArgs(const Message::Ptr &message, std::string &extraargs) {
    // Telegram ensures message does not have whitespaces beginning or ending.
    auto id = message->text.find_first_of(" \n");
    if (id != std::string::npos) {
        extraargs = message->text.substr(id + 1);
    }
}

static bool writeMessageToFile(const Bot &bot, const Message::Ptr &message,
                               const char *filename) {
    std::ofstream file(filename);
    if (file.fail()) {
        bot_sendReplyMessage(bot, message, "Failed to open file");
        return false;
    }
    file << message->replyToMessage->text;
    file.close();
    return true;
}

static void addExtArgs(std::stringstream &cmd, std::string &extraargs,
                       std::string &res) {
    if (!extraargs.empty()) {
        auto idx = extraargs.find_first_of("\n\r\t");
        if (idx != std::string::npos) extraargs.erase(idx);
        cmd << SPACE << extraargs;
        res += "cmd: \"";
        res += cmd.str();
        res += "\"\n";
    }
}

static void runCommand(const Bot &bot, const Message::Ptr &message,
                       const std::string &cmd, std::string &res, bool use_wdt = true) {
    bool hasmore = false, watchdog_bitten = false;
    POPEN_WDT_HANDLE pipefd[2] = {invalid_fd_value, invalid_fd_value};
    int count = 0;
    constexpr const static int read_buf = (1 << 8), max_buf = (read_buf << 2) * 3;
    auto buf = std::make_unique<char[]>(read_buf);
    std::string cmd_r, cmd_remapped;
    const char *cmd_cstr = nullptr;
    using std::chrono::duration_cast;
    using std::chrono::high_resolution_clock;
    using std::chrono::milliseconds;
    using std::chrono::seconds;

    if (cmd.find("\"") != std::string::npos) {
        cmd_remapped.reserve(cmd.size());
        cmd_cstr = cmd.c_str();
        for (; *cmd_cstr != '\0'; cmd_cstr++) {
            if (*cmd_cstr == '"') {
                cmd_remapped += '\"';
            } else {
                cmd_remapped += *cmd_cstr;
            }
        }
        cmd_r = cmd_remapped;
    } else {
        cmd_r = cmd;
    }

    if (use_wdt)
        InitPipeHandle(&pipefd);

    auto start = high_resolution_clock::now();
    auto fp = popen_watchdog(cmd_r.c_str(), pipefd[1]);

    if (!fp) {
        bot_sendReplyMessage(bot, message, "Failed to popen()");
        return;
    }
    res.reserve(max_buf);
    while (fgets(buf.get(), read_buf, fp)) {
        if (res.size() < max_buf) {
            res += buf.get();
        } else {
            hasmore = true;
        }
        count++;
    }
#if defined PWD_STR && defined PWD_REPLACE_STR
    size_t start_pos = 0;
    std::string pwd(PWD_STR), replace(PWD_REPLACE_STR);
    while((start_pos = res.find(pwd, start_pos)) != std::string::npos) {
        res.replace(start_pos, pwd.length(), replace);
        start_pos += replace.length();
    }
#endif
    if (count == 0)
        res += std::string() + EMPTY + '\n';
    else if (res.back() != '\n')
        res += '\n';
    res += '\n';
    if (hasmore) res += "-> Truncated due to too much output\n";
    auto end = high_resolution_clock::now();

    if (pipefd[0] != invalid_fd_value) {
        int flags;
        if (duration_cast<seconds>(end - start).count() < SLEEP_SECONDS) {
            // Disable blocking (wdt would be sleeping if we are exiting earlier)
            unblockForHandle(pipefd[0]);
        }

        watchdog_bitten = readBoolFromHandle(pipefd[0]);
        if (watchdog_bitten) {
            res += WDT_BITE_STR;
        }
        closeHandle(pipefd[0]);
        closeHandle(pipefd[1]);
    }
    if (!watchdog_bitten) {
        std::stringstream stream;
        float millis = static_cast<float>(duration_cast<milliseconds>(end - start).count());
        stream << std::fixed << std::setprecision(3) << millis * 0.001;
        res += "-> It took " + stream.str() + " seconds\n";
    }
    pclose(fp);
}

static void commonCleanup(const Bot &bot, const Message::Ptr &message,
                          std::string &res, const char *filename) {
    bot_sendReplyMessage(bot, message, res);
    if (filename) std::remove(filename);
}

static bool commonVerifyParseWrite(const Bot &bot, const Message::Ptr &message,
                                   std::string &extraargs, const char *filename) {
    bool ret = verifyMessage(bot, message);
    if (ret) {
        parseExtArgs(message, extraargs);
        ret = writeMessageToFile(bot, message, filename);
    }
    return ret;
}

template <>
void CompileRunHandler<CCppCompileHandleData>(const CCppCompileHandleData &data) {
    std::string res, extraargs;
    std::stringstream cmd;
    const char aoutname[] = "./a.out";
    bool ret;

    if (!commonVerifyParseWrite(data.bot, data.message, extraargs, data.outfile)) return;

    cmd << data.cmdPrefix << SPACE << data.outfile;
    addExtArgs(cmd, extraargs, res);
#ifdef DEBUG
    printf("cmd: %s\n", cmd.str().c_str());
#endif
    res += "Compile time:\n";
    runCommand(data.bot, data.message, cmd.str(), res);
    res += "\n";

    std::ifstream aout(aoutname);
    if (aout.good()) {
        aout.close();
        res += "Run time:\n";
        runCommand(data.bot, data.message, aoutname, res);
        std::remove(aoutname);
    }

    commonCleanup(data.bot, data.message, res, data.outfile);
}

template <>
void CompileRunHandler(const CompileHandleData &data) {
    std::string res, extargs;
    std::stringstream cmd;
    bool ret;

    if (!commonVerifyParseWrite(data.bot, data.message, extargs, data.outfile)) return;

    cmd << data.cmdPrefix << SPACE << data.outfile;
    addExtArgs(cmd, extargs, res);

#ifdef DEBUG
    printf("cmd: %s\n", cmd.str().c_str());
#endif
    runCommand(data.bot, data.message, cmd.str(), res);
    commonCleanup(data.bot, data.message, res, data.outfile);
}

template <>
void CompileRunHandler<BashHandleData>(const BashHandleData &data) {
    if (!Authorized(data.message)) return;
    std::string res;
    if (hasExtArgs(data.message)) {
        std::string cmd;
        parseExtArgs(data.message, cmd);
        runCommand(data.bot, data.message, cmd, res, !data.allowhang);
    } else {
        res = "Send a bash command to run";
    }
    commonCleanup(data.bot, data.message, res, nullptr);
}
