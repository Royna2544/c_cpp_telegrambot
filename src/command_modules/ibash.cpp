#include <BotReplyMessage.h>
#include <ExtArgs.h>
#include <command_modules/CommandModule.h>
#include <compiler/CompilerInTelegram.h>  // BASH_MAX_BUF, BASH_READ_BUF
#include <internal/_FileDescriptor_posix.h>
#include <popen_wdt/popen_wdt.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <unistd.h>

#include <ManagedThreads.hpp>
#include <MessageWrapper.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <csignal>
#include <cstdlib>
#include <functional>
#include <memory>
#include <regex>
#include <string_view>
#include <utility>

#include "../../socket/selector/SelectorPosix.hpp"
#include "BotClassBase.h"
#include "tgbot/types/Message.h"

using std::chrono_literals::operator""s;

struct InteractiveBashContext : BotClassBase {
    // parentToChild { parent write, child stdin }
    // childToParent { parent read, child stdout }
    pipe_t childToParent{}, parentToChild{};
    bool is_open = false;
    std::shared_ptr<ThreadManager> gSThreadManager;

    // Aliases
    const int& parent_readfd = childToParent[0];
    const int& parent_writefd = parentToChild[0];
    const int& child_stdin = parentToChild[1];
    const int& child_stdout = childToParent[1];
    constexpr static std::string_view kOutputInitBuf = "Output:\n";
    constexpr static std::string_view kSubProcessClosed =
        "Subprocess closed due to inactivity";

    explicit InteractiveBashContext(const Bot& bot)
        : BotClassBase(bot), gSThreadManager(ThreadManager::getInstance()) {}

    struct ExitTimeoutThread : ManagedThreadRunnable {
        void runFunction() override {
            if (delayUnlessStop(SLEEP_SECONDS); kRun) {
                if (context->childpid > 0 && kill(context->childpid, 0) == 0) {
                    LOG(WARNING) << "Process " << context->childpid
                                 << " misbehaving, using SIGTERM";
                    killpg(context->childpid, SIGTERM);
                }
            }
        }
        InteractiveBashContext* context;
        explicit ExitTimeoutThread(InteractiveBashContext* context)
            : context(context) {}
    };

    struct UpdateOutputThread : ManagedThreadRunnable, BotClassBase {
        void runFunction() override {
            PollSelector selector;
            std::array<char, BASH_MAX_BUF> buffer{};
            ssize_t offset = 0;

            selector.init();
            selector.setTimeout(100s);
            selector.enableTimeout(true);
            selector.add(readfd, [&buffer, &offset, this]() {
                auto len = read(readfd, buffer.data() + offset, BASH_READ_BUF);
                if (len < 0) {
                    PLOG(ERROR) << "Failed to read from pipe";
                    kRun = false;
                } else {
                    offset += len;
                    if (offset + BASH_READ_BUF > BASH_MAX_BUF) {
                        LOG(INFO) << "Buffer overflow";
                        kRun = false;
                    }
                }
            });
            while (kRun) {
                switch (selector.poll()) {
                    case Selector::SelectorPollResult::OK:
                        bot_editMessage(_bot, message, buffer.data());
                        selector.reinit();
                        break;
                    case Selector::SelectorPollResult::FAILED:
                        LOG(ERROR) << "Failed to read from pipe";
                        kRun = false;
                        break;
                    case Selector::SelectorPollResult::TIMEOUT:
                        LOG(INFO) << "Timeout";
                        kRun = false;
                        break;
                };
            }
        }
        explicit UpdateOutputThread(const Bot& bot, Message::Ptr message,
                                    int readfd)
            : BotClassBase(bot), readfd(readfd), message(std::move(message)) {}

       private:
        int readfd;
        Message::Ptr message;
    };

    static bool isExitCommand(const std::string& str) {
        static const std::regex kExitCommandRegex(R"(^exit( \d+)?$)");
        return std::regex_match(str, kExitCommandRegex);
    }

    struct IBashPriv {
        sem_t sem{};
        pid_t child_pid{};
        IBashPriv() {
            // Initialize semaphore for inter-process
            // use, initial value 0
            sem_init(&sem, 1, 0);
        }
        ~IBashPriv() { sem_destroy(&sem); }
    };

    static void unMap(IBashPriv* addr) { munmap(addr, sizeof(IBashPriv)); }

    bool open(const ChatId chat) {
        std::vector<std::function<void(void)>> cleanupStack;
        const auto cleanupFunc = [&cleanupStack]() {
            std::ranges::for_each(cleanupStack,
                                  [](const auto& func) { func(); });
        };

        if (is_open) {
            return false;
        }

        void* addr = mmap(nullptr, sizeof(IBashPriv), PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (addr == MAP_FAILED) {
            PLOG(ERROR) << "mmap";
            return false;
        }
        new (addr) IBashPriv();
        std::unique_ptr<IBashPriv, decltype(&unMap)> privdata(
            static_cast<IBashPriv*>(addr), &unMap);

        pid_t pid = 0;
        int ret = 0;

        InvaildatePipe(parentToChild);
        InvaildatePipe(childToParent);

        for (const auto& pipes : {&parentToChild, &childToParent}) {
            ret = pipe(*pipes);
            if (ret != 0) {
                PLOG(ERROR) << "pipe";
                cleanupFunc();
                return false;
            } else {
                // These are variables on this class.
                // and this cleanupstack will execute on this function scope
                cleanupStack.emplace_back([pipes]() { closePipe(*pipes); });
            }
        }
        pid = fork();
        if (pid < 0) {
            PLOG(ERROR) << "fork";
            cleanupFunc();
            return false;
        }
        if (pid == 0) {
            // Child
            dup2(child_stdout, STDOUT_FILENO);
            dup2(child_stdout, STDERR_FILENO);
            dup2(child_stdin, STDIN_FILENO);
            close(parent_readfd);
            close(parent_writefd);
            privdata->child_pid = getpid();
            setpgid(0, 0);
            sem_post(&privdata->sem);
            execl(BASH_EXE_PATH, "bash", nullptr);
            _exit(127);
        } else {
            close(child_stdin);
            close(child_stdout);
            // Ensure bash execl() is up
            // to be able to read from it
            sem_wait(&privdata->sem);
            childpid = privdata->child_pid;
            is_open = true;
            LOG(INFO) << "Open success, child pid: " << childpid;
        }
        auto msg = bot_sendMessage(_bot, chat, "IBash");
        gSThreadManager->getInstance()
            ->createController<ThreadManager::Usage::IBASH_UPDATE_OUTPUT_THREAD,
                               UpdateOutputThread>(

                std::cref(_bot), msg, parent_readfd)
            ->run();
        return true;
    }
    void exit(const Message::Ptr& message) {
        if (is_open) {
            int status = 0;

            // Write a msg as a fallback if for some reason exit doesnt get
            // written
            auto exitTimeout = gSThreadManager->createController<
                ThreadManager::Usage::IBASH_EXIT_TIMEOUT_THREAD,
                ExitTimeoutThread>(this);

            if (!exitTimeout) {
                return;
            }

            // Try to type exit command
            SendCommand("exit 0", true);
            // waitpid(4p) will hang if not exited, yes
            waitpid(childpid, &status, 0);
            if (WIFEXITED(status)) {
                LOG(INFO) << "Process " << childpid << " exited with code "
                          << WEXITSTATUS(status);
            }
            exitTimeout->stop();
            gSThreadManager->destroyController(ThreadManager::Usage::IBASH_EXIT_TIMEOUT_THREAD);
            gSThreadManager->destroyController(ThreadManager::Usage::IBASH_UPDATE_OUTPUT_THREAD);
            bot_sendReplyMessage(_bot, message, "Closed");
            close(parent_readfd);
            close(parent_writefd);
            childpid = -1;
            is_open = false;
        } else {
            bot_sendReplyMessage(_bot, message, "Not open");
        }
    }

    bool sendCommand(const std::string& command) {
        return SendCommand(command);
    }

   private:
    std::atomic_bool kIsProcessing;
    pid_t childpid = 0;

    [[nodiscard]] bool SendCommand(std::string str, bool internal = false) const {
        if (!internal) {
            LOG(INFO) << "Child <= Command '" << str << "'";
        }
        ssize_t rc = 0;
        auto outputThread = gSThreadManager->getController<
            ThreadManager::Usage::IBASH_UPDATE_OUTPUT_THREAD,
            UpdateOutputThread>();
        if (!outputThread->isRunning()) {
            LOG(ERROR) << "Output thread is not running, timed out";
            return false;
        }
        boost::trim(str);
        str += '\n';
        rc = write(parent_writefd, str.c_str(), str.size());
        if (rc < 0) {
            PLOG(ERROR) << "Writing text to parent fd";
            return false;
        }
        return true;
    }
};

static void InteractiveBashCommandFn(const Bot& bot,
                                     const Message::Ptr& message) {
    static InteractiveBashContext ctx(bot);
    std::string command;
    MessageWrapper wrapper(bot, message);

    if (!wrapper.hasExtraText()) {
        if (!ctx.is_open) {
            wrapper.sendMessageOnExit("Opening...");
            if (ctx.open(message->chat->id)) {
                wrapper.sendMessageOnExit("Opened bash subprocess.");
            } else {
                wrapper.sendMessageOnExit("Failed to open child process");
            }
        } else {
            wrapper.sendMessageOnExit("Bash subprocess is already running");
        }
    } else {
        wrapper.getExtraText(command);
        if (InteractiveBashContext::isExitCommand(command)) {
            if (ctx.is_open) {
                LOG(INFO) << "Received exit command: '" << command << "'";
            }
            ctx.exit(message);
        } else {
            if (ctx.is_open) {
                if (!ctx.sendCommand(command)) {
                    wrapper.sendMessageOnExit("Failed to send command");
                    ctx.exit(message);
                }
            } else {
                wrapper.sendMessageOnExit("Bash subprocess is not open");
            }
        }
    }
}

void loadcmd_ibash(CommandModule& module) {
    module.command = "ibash";
    module.description = "Interactive bash";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = InteractiveBashCommandFn;
}