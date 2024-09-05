#include <internal/_FileDescriptor_posix.h>
#include <popen_wdt.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <ManagedThreads.hpp>
#include <TgBotWrapper.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <csignal>
#include <cstdlib>
#include <functional>
#include <memory>
#include <regex>
#include <string>
#include <string_view>
#include <utility>

#include <SelectorPosix.hpp>
#include "compiler/CompilerInTelegram.hpp"  // BASH_MAX_BUF, BASH_READ_BUF

using std::chrono_literals::operator""s;

struct InteractiveBashContext {
    // parentToChild { parent write, child stdin }
    // childToParent { parent read, child stdout }
    Pipe childToParent{}, parentToChild{};
    bool is_open = false;
    std::shared_ptr<ThreadManager> ThrMgr;

    constexpr static std::string_view kOutputInitBuf = "Output:\n";
    constexpr static std::string_view kSubProcessClosed =
        "Subprocess closed due to inactivity";

    explicit InteractiveBashContext() : ThrMgr(ThreadManager::getInstance()) {}

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

    struct UpdateOutputThread : ManagedThreadRunnable {
        void runFunction() override {
            PollSelector selector;

            selector.init();
            selector.setTimeout(100s);
            selector.enableTimeout(true);
            selector.add(
                readfd,
                [this]() {
                    std::lock_guard<std::mutex> lock(m_buffer);
                    auto len =
                        read(readfd, buffer.data() + offset, BASH_READ_BUF);
                    if (len < 0) {
                        PLOG(ERROR) << "Failed to read from pipe";
                        kRun = false;
                    } else {
                        offset += len;
                        if (offset + BASH_READ_BUF > BASH_MAX_BUF) {
                            LOG(INFO) << "Buffer overflow";
                        }
                    }
                },
                Selector::Mode::READ);
            while (kRun) {
                switch (selector.poll()) {
                    case Selector::SelectorPollResult::OK:
                        TgBotWrapper::getInstance()->editMessage(message,
                                                                 buffer.data());
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
        void onNewCommand(const std::string& command) {
            std::lock_guard<std::mutex> lock(m_buffer);
            buffer.fill(0);

            std::string header = ("Output of command: " + command + "\n");
            strcpy(buffer.data(), header.c_str());
            offset = header.size();
        }
        explicit UpdateOutputThread(Message::Ptr message, int readfd)
            : readfd(readfd), message(std::move(message)) {}

       private:
        int readfd;
        Message::Ptr message;
        std::mutex m_buffer;  // Protect below 2
        size_t offset = 0;
        std::array<char, BASH_MAX_BUF> buffer{};
    };

    static bool isExitCommand(const std::string& str) {
        static const std::regex kExitCommandRegex(R"(^exit( \d+)?$)");
        return std::regex_match(str, kExitCommandRegex);
    }

    bool open(const ChatId chat) {
        std::vector<std::function<void(void)>> cleanupStack;
        const auto cleanupFunc = [&cleanupStack]() {
            std::ranges::for_each(cleanupStack,
                                  [](const auto& func) { func(); });
        };

        if (is_open) {
            return false;
        }
        pid_t pid = 0;
        int ret = 0;

        childToParent.invalidate();
        parentToChild.invalidate();

        for (const auto& pipes : {&parentToChild, &childToParent}) {
            bool ret = pipes->pipe();
            if (!ret) {
                PLOG(ERROR) << "pipe";
                cleanupFunc();
                return false;
            } else {
                // These are variables on this class.
                // and this cleanupstack will execute on this function scope
                cleanupStack.emplace_back([pipes]() { pipes->close(); });
            }
        }
        pid = vfork();
        if (pid < 0) {
            PLOG(ERROR) << "fork";
            cleanupFunc();
            return false;
        }
        if (pid == 0) {
            // Child
            dup2(childToParent.writeEnd(), STDOUT_FILENO);
            dup2(childToParent.writeEnd(), STDERR_FILENO);
            dup2(parentToChild.readEnd(), STDIN_FILENO);
            close(childToParent.readEnd());
            close(parentToChild.writeEnd());
            setpgid(0, 0);
            execl(BASH_EXE_PATH, "bash", nullptr);
            _exit(127);
        } else {
            close(childToParent.writeEnd());
            close(parentToChild.readEnd());
            is_open = true;
            childpid = pid;
            LOG(INFO) << "Open success, child pid: " << childpid;
        }
        auto msg = wrapper->sendMessage(chat, "IBash starts...");
        ThrMgr->getInstance()
            ->createController<ThreadManager::Usage::IBASH_UPDATE_OUTPUT_THREAD,
                               UpdateOutputThread>(

                msg, childToParent.readEnd())
            ->run();
        return true;
    }
    std::shared_ptr<TgBotWrapper> wrapper = TgBotWrapper::getInstance();

    void exit(const Message::Ptr& message) {
        if (is_open) {
            int status = 0;

            // Write a msg as a fallback if for some reason exit doesnt get
            // written
            auto exitTimeout = ThrMgr->createController<
                ThreadManager::Usage::IBASH_EXIT_TIMEOUT_THREAD,
                ExitTimeoutThread>(this);

            if (!exitTimeout) {
                return;
            }

            // Try to type exit command
            sendCommandNoCheck("exit 0");
            // waitpid(4p) will hang if not exited, yes
            waitpid(childpid, &status, 0);
            if (WIFEXITED(status)) {
                LOG(INFO) << "Process " << childpid << " exited with code "
                          << WEXITSTATUS(status);
            }
            ThrMgr
                ->getController<
                    ThreadManager::Usage::IBASH_UPDATE_OUTPUT_THREAD,
                    UpdateOutputThread>()
                ->stop();
            exitTimeout->stop();
            ThrMgr->destroyController(
                ThreadManager::Usage::IBASH_EXIT_TIMEOUT_THREAD);
            ThrMgr->destroyController(
                ThreadManager::Usage::IBASH_UPDATE_OUTPUT_THREAD);
            wrapper->sendReplyMessage(message, "Closed");
            close(childToParent.readEnd());
            close(parentToChild.writeEnd());
            childpid = -1;
            is_open = false;
        } else {
            wrapper->sendReplyMessage(message, "Not open");
        }
    }

    bool sendCommand(const std::string& command) {
        return SendCommand(command);
    }

   private:
    pid_t childpid = 0;

    [[nodiscard]] bool SendCommand(std::string str,
                                   bool internal = false) const {
        if (!internal) {
            LOG(INFO) << "Child <= Command '" << str << "'";
        }
        ssize_t rc = 0;
        auto outputThread = ThrMgr->getController<
            ThreadManager::Usage::IBASH_UPDATE_OUTPUT_THREAD,
            UpdateOutputThread>();
        if (!outputThread->isRunning()) {
            LOG(ERROR) << "Output thread is not running, timed out";
            return false;
        }
        outputThread->onNewCommand(str);
        return sendCommandNoCheck(str);
    }

    [[nodiscard]] bool sendCommandNoCheck(std::string str) const {
        ssize_t rc = 0;
        boost::trim(str);
        str += '\n';
        rc = write(parentToChild.writeEnd(), str.c_str(), str.size());
        if (rc < 0) {
            PLOG(ERROR) << "Writing text to parent fd";
            return false;
        }
        return true;
    }
};

DECLARE_COMMAND_HANDLER(ibash, bot, message) {
    static InteractiveBashContext ctx;
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

DYN_COMMAND_FN(n, module) {
    module.command = "ibash";
    module.description = "Interactive bash";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = COMMAND_HANDLER_NAME(ibash);
    return true;
}