#include <BotReplyMessage.h>
#include <ExtArgs.h>
#include <Logging.h>
#include <fcntl.h>
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
    return fd && (fd[0] >= 0 && fd[1] >= 0);
}

static void InvaildatePipe(pipe_t fd) {
    if (fd) {
        fd[0] = -1;
        fd[1] = -1;
    }
}

static bool SendCommand(std::string str, bool internal = false) {
    int rc;

    TrimStr(str);
    if (!internal)
        LOG_I("Child <= Command '%s'", str.c_str());
    // To stop the read() when the command exit
    str += "; echo\n";
    rc = write(parent_writefd, str.c_str(), str.size() + /* null terminator */ 1);
    if (rc < 0) {
        PLOG_E("Write command to parent fd");
        return false;
    }
    return true;
}

// TODO This is broken (Will hang if its not interactive)
static bool isChildInteractive() {
    static const char OkStr[] = "OK\n";
    constexpr size_t OkStrLen = sizeof(OkStr);
    char buf[OkStrLen] = {0};
    int rc;

    // Send a command to check connection to child bash process
    rc = SendCommand("echo OK", true);
    if (!rc) {
        return false;
    }
    // Expecting "OK\n"
    return read(parent_readfd, &buf, OkStrLen) == OkStrLen && !strncmp(buf, OkStr, OkStrLen - 1);
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
            perror("mmap");
            return;
        }

        *piddata = 0;
        InvaildatePipe(tochild);
        InvaildatePipe(toparent);

        // (-1 || -1) is 1, (-1 || 0) is 1, (0 || 0) is 0
        rc = pipe(tochild) || pipe(toparent);

        if (rc) {
            perror("pipe");
            return;
        }
        if ((pid = fork()) < 0) {
            perror("fork");
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
            close(child_stdin);
            if (isChildInteractive()) {
                is_open = true;
                childpid = *piddata;
                LOG_D("Open success, child pid: %d", childpid);
                munmap(piddata, sizeof(pid_t));
                bot_sendReplyMessage(bot, message, "Opened bash subprocess.");
            } else {
                LOG_W("Write failed, process not opened!");
            }
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
                        bot_sendReplyMessage(bot, message, "Subprocess is still running, try using an exit command");
                        return;
                    }
                }
                std::thread sendResThread([&bot, message, command] {
                    static const char kExitedByTimeout[] = WDT_BITE_STR;
                    char buf[BASH_READ_BUF];
                    int rc, count = 0;
                    bool once_flag = true;
                    std::string result;
                    auto sendFallback = std::make_shared<std::atomic_bool>(true);
                    const std::lock_guard<std::mutex> _(m);

                    SendCommand(command);

                    // Write a msg as a fallback if read() hangs
                    std::thread th([sendFallback] {
                        std_sleep_s(SLEEP_SECONDS);
                        if (sendFallback->load())
                            (void)write(child_stdout, kExitedByTimeout, sizeof(kExitedByTimeout));
                    });
                    // When rc < 0, most likely it returned EWOUDLBLOCK/EAGAIN
                    do {
                        rc = read(parent_readfd, buf, sizeof(buf));
                        if (rc > 0) {
                            std::string buf_str(buf, rc);
                            // Cuz of the shim of "\n", since it is a sperate process,
                            // may not be read in the same buffer. TODO Remove dirty way
                            if (isEmptyOrBlank(buf_str) && once_flag) {
                                // TODO This is a hack
                                rc = sizeof(buf);
                                once_flag = false;
                            } else if (count < BASH_MAX_BUF) {
                                result.append(buf_str);
                                count += rc;
                            }
                        }
                        // Exit if read ret != buf size
                    } while (rc > 0 && rc == sizeof(buf));

                    // Was it the fallback input?
                    if (strncmp(buf, kExitedByTimeout, sizeof(buf))) {
                        // No? then set to not write fallback and detach
                        sendFallback->store(false);
                        th.detach();
                    } else {
                        // Yes? Do join then
                        if (th.joinable())
                            th.join();
                    }

                    if (isEmptyOrBlank(result))
                        result = "(No output)";
                    bot_sendReplyMessage(bot, message, result, 0, true);
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
