#include <BotReplyMessage.h>
#include <ExtArgs.h>
#include <Logging.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <mutex>
#include <regex>

#include "../ExtArgs.cpp"
#include "CompilerInTelegram.h"  // BASH_MAX_BUF, BASH_READ_BUF
#include "NamespaceImport.h"
#include "StringToolsExt.h"
#include "cmd_dynamic.h"
#include "popen_wdt/popen_wdt.h"

using pipe_t = int[2];

// tochild { child stdin , parent write }
// toparent { parent read, child stdout }
static pipe_t tochild, toparent;
static pid_t childpid = 0;
static bool is_open = false;

// Aliases
static const int& child_stdin = tochild[0];
static const int& child_stdout = toparent[1];
static const int& parent_readfd = toparent[0];
static const int& parent_writefd = tochild[1];

static bool IsVaildPipe(pipe_t fd) {
    return fd[0] >= 0 && fd[1] >= 0;
}

static void InvaildatePipe(pipe_t fd) {
    fd[0] = -1;
    fd[1] = -1;
}

static bool SendCommand(std::string str, bool internal = false) {
    int rc;

    TrimStr(str);
    if (!internal) {
        LOG_I("Child <= Command '%s'", str.c_str());
    }
    str += "\n";
    rc = write(parent_writefd, str.c_str(), str.size());
    if (rc < 0) {
        PLOG_E("Write command to parent fd");
        return false;
    }
    return true;
}

static bool SendText(std::string str) {
    int rc;

    TrimStr(str);
    LOG_I("Child <= Text '%s'", str.c_str());
    rc = write(parent_writefd, str.c_str(), str.size());
    if (rc < 0) {
        PLOG_E("Write txt to parent fd");
        return false;
    }
    return true;
}

enum ChildDataDirection : short {
    DATA_IN = POLLIN,
    DATA_OUT = POLLOUT,
};

static bool HasData(ChildDataDirection direction, bool wait = false) {
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

static void do_InteractiveBash(const Bot& bot, const Message::Ptr& message) {
    static const std::regex kExitCommandRegex(R"(^exit( \d+)?$)");
    std::string command;
    static std::mutex m;
    bool matchesExit = false;

    if (!hasExtArgs(message)) {
        bot_sendReplyMessage(bot, message, "Send a bash command along /ibash");
        return;
    } else {
        parseExtArgs(message, command);
        matchesExit = std::regex_match(command, kExitCommandRegex);
        if (matchesExit && !is_open) {
            bot_sendReplyMessage(bot, message, "Bash subprocess is not open yet");
            return;
        }
    }
    if (!is_open) {
        pid_t pid;
        int rc;
        pid_t* piddata = (pid_t*)mmap(NULL, sizeof(pid_t), PROT_READ | PROT_WRITE,
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
            PLOG_E("pipe");
            return;
        }
        if ((pid = fork()) < 0) {
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
                LOG_W("Waiting for subprocess up... (%d)", count);
                std_sleep_s(1);
                count++;
            } while (kill(-(*piddata), 0) != 0);
            is_open = true;
            childpid = *piddata;
            LOG_D("Open success, child pid: %d", childpid);
            munmap(piddata, sizeof(pid_t));
            bot_sendReplyMessage(bot, message, "Opened bash subprocess.");
        }
    }

    if (IsVaildPipe(tochild) && IsVaildPipe(toparent)) {
        // Is it exit command?
        if (matchesExit) {
            int status;
            LOG_I("Received exit command: '%s'", command.c_str());

            // Write a msg as a fallback if for some reason exit doesnt get written
            std::thread th([] {
                std_sleep_s(SLEEP_SECONDS);
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
                bot_sendReplyMessage(bot, message, "Closed bash subprocess.");
            }
            close(parent_readfd);
            close(parent_writefd);
            close(child_stdout);
            childpid = -1;
            is_open = false;
        } else {
            // Second try, it should have been opened by now or else its a failure
            if (is_open) {
                {
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
                        return;
                    }
                }
                std::thread sendResThread([&bot, message, command] {
                    static const char kSubProcessClosed[] = "Subprocess closed due to inactivity";
                    char buf[BASH_READ_BUF];
                    std::string result = "Output:\n";
                    bool resModified = false;
                    auto sendFallback = std::make_shared<std::atomic_bool>(true);
                    Message::Ptr resultMessage;
                    const std::lock_guard<std::mutex> _(m);

                    SendCommand(command);

                    // Write a msg as a fallback if read() hangs
                    std::thread th([sendFallback] {
                        std_sleep_s(300);  // 5 Mins
                        // TODO This should be sent as sperate message
                        if (sendFallback->load())
                            (void)write(child_stdout, kSubProcessClosed, sizeof(kSubProcessClosed));
                    });
                    resultMessage = bot_sendReplyMessage(bot, message, result);
                    do {
                        int rc = read(parent_readfd, buf, sizeof(buf));
                        if (rc > 0) {
                            std::string buf_str(buf, rc);
                            TrimStr(buf_str);
                            if (!buf_str.empty()) {
                                resModified = true;
                                result.append(buf_str);
                                bot_editMessage(bot, resultMessage, result);
                            }
                        }
                        // Exit if child has nothing more to send to us, or it is reading stdin
                    } while (HasData(DATA_IN) || !HasData(DATA_OUT));
                    // Was it the fallback input?
                    if (strncmp(buf, kSubProcessClosed, sizeof(buf))) {
                        // No? then set to not write fallback and detach
                        sendFallback->store(false);
                        th.detach();
                    } else {
                        // Yes? Do join then
                        if (th.joinable())
                            th.detach();
                    }
                    TrimStr(result);
                    if (!resModified) {
                        result = "(No output)";
                        bot_editMessage(bot, resultMessage, result);
                    }
                });
                sendResThread.detach();
            } else {
                bot_sendReplyMessage(bot, message, "Failed to open child process");
            }
        }
    } else {
        LOG_E("Pipes are not vaild, this is a BUG");
    }
}

static bool isSupported(void) {
    return access(BASH_EXE_PATH, R_OK | X_OK) == 0;
}

DECL_DYN_ENFORCED_COMMAND("ibash", do_InteractiveBash, isSupported);
