#include <BotReplyMessage.h>
#include <ExtArgs.h>
#include <Logging.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <regex>
#include <thread>

#include "CompilerInTelegram.h"  // BASH_MAX_BUF, BASH_READ_BUF
#include "ExtArgs.h"
#include "SingleThreadCtrl.h"
#include "StringToolsExt.h"
#include "command_modules/CommandModule.h"
#include "popen_wdt/popen_wdt.h"

using std::chrono_literals::operator""s;

struct TimeoutThread : SingleThreadCtrl {
    void start(const std::function<void(void)>& callback_fn) {
        std::unique_lock<std::mutex> lk(CV_m);
        if (cv.wait_for(lk, std::chrono::seconds(SLEEP_SECONDS)) == std::cv_status::timeout) {
            callback_fn();
        }
    }
};

struct InteractiveBashContext {
    // tochild { child stdin , parent write }
    // toparent { parent read, child stdout }
    pipe_t tochild, toparent;
    bool is_open = false;

    // Aliases
    const int& child_stdin = tochild[0];
    const int& child_stdout = toparent[1];
    const int& parent_readfd = toparent[0];
    const int& parent_writefd = tochild[1];
    constexpr static const char kOutputInitBuf[] = "Output:\n";
    constexpr static const char kSubProcessClosed[] = "Subprocess closed due to inactivity";
    constexpr static const char kNoOutputFallback[] = "\n";

    static bool isExitCommand(const std::string& str) {
        static const std::regex kExitCommandRegex(R"(^exit( \d+)?$)");
        return std::regex_match(str, kExitCommandRegex);
    }

    void open(void) {
        if (!is_open) {
            pid_t pid;
            int rc;
            pid_t* piddata;

            piddata = (pid_t*)mmap(NULL, sizeof(pid_t), PROT_READ | PROT_WRITE,
                                   MAP_SHARED | MAP_ANONYMOUS, -1, 0);
            if (piddata == MAP_FAILED) {
                PLOG_E("mmap");
                return;
            }

            *piddata = 0;
            InvaildatePipe(tochild);
            InvaildatePipe(toparent);

            // (-1 || -1) is 1, (-1 || 0) is 1, (0 || 0) is 0
            rc = pipe(tochild) || pipe(toparent);

            if (rc) {
                munmap(piddata, sizeof(pid_t));
                closePipe(tochild);
                closePipe(toparent);
                PLOG_E("pipe");
                return;
            }
            if ((pid = fork()) < 0) {
                munmap(piddata, sizeof(pid_t));
                closePipe(tochild);
                closePipe(toparent);
                PLOG_E("fork");
                return;
            }
            if (pid == 0) {
                // Child
                dup2(child_stdout, STDOUT_FILENO);
                dup2(child_stdout, STDERR_FILENO);
                dup2(child_stdin, STDIN_FILENO);
                close(parent_readfd);
                close(parent_writefd);
                *piddata = getpid();
                setpgid(0, 0);
                execl(BASH_EXE_PATH, "bash", (char*)NULL);
                _exit(127);
            } else {
                int count = 0;
                close(child_stdin);
                // Ensure bash execl() is up
                do {
                    LOG_W("Waiting for subprocess up... (%ds passed)", count);
                    std::this_thread::sleep_for(1s);
                    count++;
                } while (kill(-(*piddata), 0) != 0);
                is_open = true;
                childpid = *piddata;
                LOG_D("Open success, child pid: %d", childpid);
                munmap(piddata, sizeof(pid_t));
            }
        }
    }
    void exit(const std::function<void(void)> onClosed, const std::function<void(void)> onNotOpen) {
        if (is_open) {
            int status;

            // Write a msg as a fallback if for some reason exit doesnt get written
            std::thread th([this] {
                std::this_thread::sleep_for(std::chrono::seconds(SLEEP_SECONDS));
                if (childpid > 0) {
                    if (kill(childpid, 0) == 0) {
                        LOG_W("Process %d misbehaving, using SIGTERM", childpid);
                        killpg(childpid, SIGTERM);
                    }
                }
            });
            // No need to have it linked
            th.detach();
            // Try to type exit command
            SendCommand("exit 0", true);
            // waitpid(4p) will hang if not exited, yes
            waitpid(childpid, &status, 0);
            if (WIFEXITED(status)) {
                LOG_I("Process %d exited with code %d", childpid, WEXITSTATUS(status));
            }
            {
                const std::lock_guard<std::mutex> _(m);
                onClosed();
            }
            close(parent_readfd);
            close(parent_writefd);
            close(child_stdout);
            childpid = -1;
            is_open = false;
        } else {
            onNotOpen();
        }
    }

    bool sendText(const Bot& bot, const Message::Ptr& message) {
        if (is_open) {
            std::unique_lock<std::mutex> lk{m, std::try_to_lock};
            if (!lk.owns_lock()) {
                // TODO better verification
                if (message->replyToMessage && message->replyToMessage->from->id == bot.getApi().getMe()->id) {
                    // TODO Extract actual command
                    SendText(message->text);
                    bot_sendReplyMessage(bot, message, "Sent the text to subprocess");
                } else {
                    bot_sendReplyMessage(bot, message, "Subprocess is still running, try using an exit command");
                }
                return true;  // Handled
            }
        }
        return false;
    }

    void sendCommand(const std::string& command, std::function<void(const std::string&)> onNewResultBuffer,
                     const std::function<void(void)> onTimeout) {
        char buf[BASH_READ_BUF];
        bool resModified = false;
        std::string result = kOutputInitBuf;
        const std::lock_guard<std::mutex> _(m);

        SendCommand(command);
        const auto onNoOutputCallbackFn = [onTimeout, this]() {
            (void)write(child_stdout, kNoOutputFallback, sizeof(kNoOutputFallback));
        };
        auto onNoOutputThread = gSThreadManager.getController<TimeoutThread>
            (SingleThreadCtrlManager::USAGE_IBASH_TIMEOUT_THREAD);

        do {
            onNoOutputThread.reset();
            onNoOutputThread->setThreadFunction(std::bind(&TimeoutThread::start, onNoOutputThread, onNoOutputCallbackFn));
            ssize_t rc = read(parent_readfd, buf, sizeof(buf));
            onNoOutputThread->stop();
            if (rc > 0) {
                std::string buf_str(buf, rc);
                TrimStr(buf_str);
                if (!buf_str.empty()) {
                    resModified = true;
                    result.append(buf_str);
                    onNewResultBuffer(result);
                }
            } else 
                break;
            // Exit if child has nothing more to send to us, or it is reading stdin
        } while (HasData(DATA_IN) || !HasData(DATA_OUT));
        gSThreadManager.destroyController(SingleThreadCtrlManager::USAGE_IBASH_TIMEOUT_THREAD);
        TrimStr(result);
        if (!resModified) {
            result = "(No output)";
            onNewResultBuffer(result);
        }
    }

   private:
    std::mutex m;
    pid_t childpid = 0;

    enum ChildDataDirection : short {
        DATA_IN = POLLIN,
        DATA_OUT = POLLOUT,
    };

    bool _SendSomething(std::string str) const {
        int rc;

        TrimStr(str);
        str += '\n';
        rc = write(parent_writefd, str.c_str(), str.size());
        if (rc < 0) {
            PLOG_E("Writing text to parent fd");
            return false;
        }
        return true;
    }

    bool SendCommand(const std::string& str, bool internal = false) const {
        if (!internal) {
            LOG_I("Child <= Command '%s'", str.c_str());
        }
        return _SendSomething(str);
    }

    bool SendText(const std::string& str) const {
        LOG_I("Child <= Text '%s'", str.c_str());
        return _SendSomething(str);
    }

    bool HasData(ChildDataDirection direction, bool wait = false) const {
        int rc;
        struct pollfd poll_fd = {
            .events = direction,
            .revents = 0,
        };

        switch (direction) {
            case DATA_IN:
                poll_fd.fd = parent_readfd;
                break;
            case DATA_OUT:
                poll_fd.fd = parent_writefd;
                break;
        };
        rc = poll(&poll_fd, 1, wait ? -1 : 100);
        if (rc < 0) {
            PLOG_E("Poll failed");
        } else {
            if (poll_fd.revents & direction) {
                return true;
            }
        }
        return false;
    }
};

static void InteractiveBashCommandFn(const Bot& bot, const Message::Ptr& message) {
    static InteractiveBashContext ctx;
    std::string command;

    if (!hasExtArgs(message)) {
        bot_sendReplyMessage(bot, message, "Send a bash command along /ibash");
    } else {
        parseExtArgs(message, command);
        if (InteractiveBashContext::isExitCommand(command)) {
            if (ctx.is_open)
                LOG_I("Received exit command: '%s'", command.c_str());
            ctx.exit(
                [&bot, message]() { bot_sendReplyMessage(bot, message, "Closed bash subprocess."); },
                [&bot, message]() { bot_sendReplyMessage(bot, message, "Bash subprocess is not open yet"); });
        } else {
            if (!ctx.is_open) {
                ctx.open();
                if (ctx.is_open)
                    bot_sendReplyMessage(bot, message, "Opened bash subprocess.");
                else
                    bot_sendReplyMessage(bot, message, "Failed to open child process");
            }
            ASSERT(IsValidPipe(ctx.tochild) && IsValidPipe(ctx.toparent), "Opening pipes failed");

            do {
                if (ctx.sendText(bot, message))
                    break;
                auto resultMessage = bot_sendReplyMessage(bot, message, InteractiveBashContext::kOutputInitBuf);
                ctx.sendCommand(
                    command, 
                    [&bot, resultMessage](const std::string& buf) { bot_editMessage(bot, resultMessage, buf); },
                    [&bot, message] { bot_sendMessage(bot, message->from->id, InteractiveBashContext::kSubProcessClosed); }
                );
            } while (false);
        }
    }
}

struct CommandModule cmd_ibash {
    .enforced = true,
    .name = "ibash",
    .fn = InteractiveBashCommandFn,
};