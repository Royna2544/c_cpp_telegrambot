#include <absl/log/log.h>
#include <fmt/format.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <api/TgBotApi.hpp>
#include <cerrno>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
#error "Interactive bash is not supported on Windows"
#endif

namespace {

constexpr size_t kReadBufferSize = 4096;
constexpr size_t kMaxOutputSize = 4096;

struct BashSession {
    pid_t pid = -1;
    int stdin_fd = -1;
    int stdout_fd = -1;
    bool running = false;

    ~BashSession() { cleanup(); }

    void cleanup() {
        if (stdin_fd != -1) {
            close(stdin_fd);
            stdin_fd = -1;
        }
        if (stdout_fd != -1) {
            close(stdout_fd);
            stdout_fd = -1;
        }
        if (running && pid > 0) {
            kill(pid, SIGTERM);
            waitpid(pid, nullptr, 0);
            running = false;
        }
        pid = -1;
    }

    bool start() {
        int stdin_pipe[2];
        int stdout_pipe[2];

        if (pipe(stdin_pipe) == -1) {
            LOG(ERROR) << "Failed to create stdin pipe: " << strerror(errno);
            return false;
        }

        if (pipe(stdout_pipe) == -1) {
            LOG(ERROR) << "Failed to create stdout pipe: "
                       << strerror(errno);
            close(stdin_pipe[0]);
            close(stdin_pipe[1]);
            return false;
        }

        pid = fork();
        if (pid == -1) {
            LOG(ERROR) << "Failed to fork: " << strerror(errno);
            close(stdin_pipe[0]);
            close(stdin_pipe[1]);
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);
            return false;
        }

        if (pid == 0) {
            close(stdin_pipe[1]);
            close(stdout_pipe[0]);

            dup2(stdin_pipe[0], STDIN_FILENO);
            dup2(stdout_pipe[1], STDOUT_FILENO);
            dup2(stdout_pipe[1], STDERR_FILENO);

            close(stdin_pipe[0]);
            close(stdout_pipe[1]);

            setenv("PS1", "", 1);
            setenv("PS2", "", 1);

            execlp("bash", "bash", "--norc", "--noprofile", "-i", nullptr);
            _exit(127);
        }

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        stdin_fd = stdin_pipe[1];
        stdout_fd = stdout_pipe[0];
        running = true;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        return true;
    }

    bool writeCommand(const std::string& command) {
        if (!running || stdin_fd == -1) {
            return false;
        }

        std::string cmd = command + "\n";
        ssize_t written = write(stdin_fd, cmd.c_str(), cmd.size());
        if (written == -1) {
            LOG(ERROR) << "Failed to write to bash stdin: "
                       << strerror(errno);
            return false;
        }

        return true;
    }

    std::optional<std::string> readOutput() {
        if (!running || stdout_fd == -1) {
            return std::nullopt;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(stdout_fd, &read_fds);

        struct timeval timeout {};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ret = select(stdout_fd + 1, &read_fds, nullptr, nullptr, &timeout);
        if (ret == -1) {
            LOG(ERROR) << "select() failed: " << strerror(errno);
            return std::nullopt;
        }

        if (ret == 0) {
            return std::string("(no output)");
        }

        std::string output;
        char buffer[kReadBufferSize];

        while (true) {
            FD_ZERO(&read_fds);
            FD_SET(stdout_fd, &read_fds);
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;

            ret = select(stdout_fd + 1, &read_fds, nullptr, nullptr, &timeout);
            if (ret <= 0) {
                break;
            }

            ssize_t n = read(stdout_fd, buffer, sizeof(buffer) - 1);
            if (n <= 0) {
                break;
            }

            buffer[n] = '\0';
            output.append(buffer, n);

            if (output.size() >= kMaxOutputSize) {
                output += "\n[Output truncated]";
                break;
            }
        }

        if (output.empty()) {
            return std::string("(no output)");
        }

        return output;
    }

    bool isRunning() {
        if (!running || pid == -1) {
            return false;
        }

        int status;
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == 0) {
            return true;
        }

        if (result == pid) {
            running = false;
            LOG(INFO) << "Bash process exited with status: " << status;
            return false;
        }

        return running;
    }
};

class InteractiveBashManager {
   public:
    using ChatId = std::int64_t;

    static InteractiveBashManager& getInstance() {
        static InteractiveBashManager instance;
        return instance;
    }

    bool startSession(ChatId chatId) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (sessions_.find(chatId) != sessions_.end()) {
            if (sessions_[chatId]->isRunning()) {
                return false;
            }
            sessions_.erase(chatId);
        }

        auto session = std::make_unique<BashSession>();
        if (!session->start()) {
            return false;
        }

        sessions_[chatId] = std::move(session);
        return true;
    }

    bool hasSession(ChatId chatId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(chatId);
        if (it == sessions_.end()) {
            return false;
        }
        return it->second->isRunning();
    }

    std::optional<std::string> executeCommand(ChatId chatId,
                                              const std::string& command) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = sessions_.find(chatId);
        if (it == sessions_.end() || !it->second->isRunning()) {
            return std::nullopt;
        }

        if (!it->second->writeCommand(command)) {
            return std::nullopt;
        }

        return it->second->readOutput();
    }

    bool endSession(ChatId chatId) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = sessions_.find(chatId);
        if (it == sessions_.end()) {
            return false;
        }

        it->second->cleanup();
        sessions_.erase(it);
        return true;
    }

   private:
    InteractiveBashManager() = default;
    ~InteractiveBashManager() = default;

    InteractiveBashManager(const InteractiveBashManager&) = delete;
    InteractiveBashManager& operator=(const InteractiveBashManager&) = delete;

    std::mutex mutex_;
    std::map<ChatId, std::unique_ptr<BashSession>> sessions_;
};

DECLARE_COMMAND_HANDLER(ibash) {
    auto& manager = InteractiveBashManager::getInstance();
    auto chatId = message->get<MessageAttrs::Chat>()->id;

    if (!message->has<MessageAttrs::ExtraText>()) {
        if (manager.startSession(chatId)) {
            api->sendReplyMessage(
                message->message(),
                "Interactive bash session started. Use /ibash <command> to "
                "execute commands.\nUse /ibash exit to close the session.");
        } else {
            api->sendReplyMessage(
                message->message(),
                "Failed to start session or session already exists.");
        }
        return;
    }

    auto command = message->get<MessageAttrs::ExtraText>();

    if (command.starts_with("exit")) {
        if (manager.endSession(chatId)) {
            api->sendReplyMessage(message->message(),
                                  "Interactive bash session ended.");
        } else {
            api->sendReplyMessage(message->message(), "No active session.");
        }
        return;
    }

    if (!manager.hasSession(chatId)) {
        api->sendReplyMessage(
            message->message(),
            "No active session. Start one with /ibash first.");
        return;
    }

    auto output = manager.executeCommand(chatId, command);
    if (output.has_value()) {
        if (output->empty()) {
            api->sendReplyMessage(message->message(), "(no output)");
        } else {
            api->sendReplyMessage(message->message(), *output);
        }
    } else {
        api->sendReplyMessage(message->message(),
                              "Failed to execute command or session ended.");
        manager.endSession(chatId);
    }
}

}  // namespace

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "ibash",
    .description = "Interactive bash shell",
    .function = COMMAND_HANDLER_NAME(ibash),
    .valid_args =
        {
            .enabled = true,
            .counts = DynModule::craftArgCountMask<0, 1>(),
            .split_type = DynModule::ValidArgs::Split::ByWhitespace,
            .usage = "/ibash - Start session\n/ibash <command> - Execute "
                     "command\n/ibash exit - End session",
        },
};
