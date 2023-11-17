#include <Authorization.h>
#include <BotReplyMessage.h>
#include <CompilerInTelegram.h>
#include <ExtArgs.h>
#include <Logging.h>
#include <NamespaceImport.h>
#include <utils/libutils.h>

#include <chrono>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

#include "popen_wdt/popen_wdt.h"

using std::chrono_literals::operator""ms;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

static const char SPACE = ' ';
static const char EMPTY[] = "(empty)";

static bool verifyMessage(const Bot &bot, const Message::Ptr &message) {
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
                               const std::string &filename) {
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
    int count = 0;
    constexpr const static int read_buf = (1 << 8), max_buf = (read_buf << 2) * 3;
    auto buf = std::make_unique<char[]>(read_buf);
    std::string cmd_r, cmd_remapped;
    const char *cmd_cstr = nullptr;

#ifdef LOCALE_EN_US
    static std::once_flag once;
    std::call_once(once, [] {
        setlocale_enus_once();
    });
#endif

    if (cmd.find("\"") != std::string::npos) {
        cmd_remapped.reserve(cmd.size());
        for (cmd_cstr = cmd.c_str(); *cmd_cstr != '\0'; cmd_cstr++) {
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

    LOG_I("Command: %s", cmd_r.c_str());
    auto start = high_resolution_clock::now();
    auto fp = popen_watchdog(cmd_r.c_str(), use_wdt ? &watchdog_bitten : nullptr);

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
        std::this_thread::sleep_for(50ms);
    }
#if defined PWD_REPLACE_STR && !defined __WIN32
    size_t start_pos = 0;
    std::string pwd(getSrcRoot()), replace(PWD_REPLACE_STR);
    while ((start_pos = res.find(pwd, start_pos)) != std::string::npos) {
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

    if (watchdog_bitten) {
        res += WDT_BITE_STR;
    } else {
        std::stringstream stream;
        float millis = static_cast<float>(duration_cast<milliseconds>(end - start).count());
        stream << std::fixed << std::setprecision(3) << millis * 0.001;
        res += "-> It took " + stream.str() + " seconds\n";
    }
    pclose(fp);
}

static void commonCleanup(const Bot &bot, const Message::Ptr &message,
                          std::string &res, const std::string &filename = "") {
    bot_sendReplyMessage(bot, message, res);
    if (!filename.empty()) std::remove(filename.c_str());
}

// Verify, Parse, Write
static bool commonVPW(const CompileHandleData &data, std::string &extraargs) {
    bool ret = false;
    if (hasExtArgs(data.message)) {
        parseExtArgs(data.message, extraargs);
        if (extraargs == "--path") {
            bot_sendReplyMessage(data.bot, data.message,
                                 "Selected compiler: " + data.cmdPrefix);
            return ret;  // Bail out
        }
    }
    ret = verifyMessage(data.bot, data.message) &&
          writeMessageToFile(data.bot, data.message, data.outfile);
    return ret;
}

template <>
void CompileRunHandler<CCppCompileHandleData>(const CCppCompileHandleData &data) {
    std::string res, extraargs;
    std::stringstream cmd;
    const char *aoutname = IS_DEFINED(__WIN32) ? "./a.exe" : "./a.out";

    if (commonVPW(data, extraargs)) {
        cmd << data.cmdPrefix << SPACE << data.outfile;
        addExtArgs(cmd, extraargs, res);

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
}

template <>
void CompileRunHandler(const CompileHandleData &data) {
    std::string res, extargs;
    std::stringstream cmd;

    if (commonVPW(data, extargs)) {
        cmd << data.cmdPrefix << SPACE << data.outfile;
        addExtArgs(cmd, extargs, res);

        runCommand(data.bot, data.message, cmd.str(), res);
        commonCleanup(data.bot, data.message, res, data.outfile);
    }
}

template <>
void CompileRunHandler<BashHandleData>(const BashHandleData &data) {
    std::string res;
    if (hasExtArgs(data.message)) {
        std::string cmd;
        parseExtArgs(data.message, cmd);
        runCommand(data.bot, data.message, cmd, res, !data.allowhang);
    } else {
        res = "Send a bash command to run";
    }
    commonCleanup(data.bot, data.message, res);
}
