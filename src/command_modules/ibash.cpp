#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <absl/log/log.h>
#include <fcntl.h>
#include <fmt/format.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <api/StringResLoader.hpp>
#include <api/TgBotApi.hpp>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <compare>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#error "Interactive bash is not supported on Windows"
#endif

extern char** environ;

namespace {

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr std::size_t kReadBufferSize = 4096;
constexpr std::size_t kMaxOutputSize = 4096;
constexpr std::size_t kMaxCommandSize = 16 * 1024;
constexpr std::size_t kMaxSessions = 16;
constexpr auto kCommandDeadline = 8s;
constexpr auto kIdleTimeout = std::chrono::minutes(10);
constexpr auto kReaperInterval = 1s;
constexpr auto kTerminateGrace = 2s;
constexpr auto kKillWait = 2s;
constexpr auto kPollInterval = 100ms;

std::int64_t steadyMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               Clock::now().time_since_epoch())
        .count();
}

int pollTimeout(Clock::time_point deadline) {
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                              Clock::now());
    if (remaining <= 0ms) {
        return 0;
    }
    return static_cast<int>(
        std::max<std::int64_t>(1, std::min(remaining, kPollInterval).count()));
}

bool setCloseOnExec(int fd) {
    const int flags = fcntl(fd, F_GETFD);
    return flags != -1 && fcntl(fd, F_SETFD, flags | FD_CLOEXEC) != -1;
}

bool makePipe(int pipe_fds[2]) {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__ANDROID__)
    if (pipe2(pipe_fds, O_CLOEXEC) == 0) {
        return true;
    }
    if (errno != ENOSYS && errno != EINVAL) {
        return false;
    }
#endif
    if (pipe(pipe_fds) == -1) {
        return false;
    }
    if (setCloseOnExec(pipe_fds[0]) && setCloseOnExec(pipe_fds[1])) {
        return true;
    }
    const int saved_errno = errno;
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    pipe_fds[0] = -1;
    pipe_fds[1] = -1;
    errno = saved_errno;
    return false;
}

bool setNonBlocking(int fd) {
    const int flags = fcntl(fd, F_GETFL);
    return flags != -1 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

ssize_t writeWithoutSigpipe(int fd, const void* data, std::size_t size) {
    sigset_t blocked{};
    sigset_t old_mask{};
    sigset_t pending{};
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &blocked, &old_mask) != 0) {
        return -1;
    }

    const bool already_pending =
        sigpending(&pending) == 0 && sigismember(&pending, SIGPIPE) == 1;
    const auto result = write(fd, data, size);
    const int saved_errno = errno;
    if (result == -1 && saved_errno == EPIPE && !already_pending) {
        int delivered = 0;
        (void)sigwait(&blocked, &delivered);
    }
    (void)pthread_sigmask(SIG_SETMASK, &old_mask, nullptr);
    errno = saved_errno;
    return result;
}

std::string_view trimAscii(std::string_view value) {
    constexpr std::string_view whitespace = " \t\r\n\f\v";
    const auto first = value.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

std::vector<std::string> childEnvironmentStorage() {
    std::vector<std::string> result;
    for (char** entry = environ; entry != nullptr && *entry != nullptr;
         ++entry) {
        const std::string_view value(*entry);
        if (!value.starts_with("PS1=") && !value.starts_with("PS2=")) {
            result.emplace_back(value);
        }
    }
    result.emplace_back("PS1=");
    result.emplace_back("PS2=");
    return result;
}

struct SessionKey {
    std::int64_t chat{};
    std::int64_t user{};

    auto operator<=>(const SessionKey&) const = default;
};

class BashSession {
   public:
    ~BashSession() { cleanup(); }

    bool start(std::stop_token stop, Clock::time_point deadline) {
        std::scoped_lock lock(mutex_);
        if (running_ || stop.stop_requested() || Clock::now() >= deadline) {
            return false;
        }

        int stdin_pipe[2] = {-1, -1};
        int stdout_pipe[2] = {-1, -1};
        if (!makePipe(stdin_pipe) || !makePipe(stdout_pipe)) {
            const int saved_errno = errno;
            if (stdin_pipe[0] != -1)
                close(stdin_pipe[0]);
            if (stdin_pipe[1] != -1)
                close(stdin_pipe[1]);
            if (stdout_pipe[0] != -1)
                close(stdout_pipe[0]);
            if (stdout_pipe[1] != -1)
                close(stdout_pipe[1]);
            LOG(ERROR) << "Failed to create ibash pipes: "
                       << strerror(saved_errno);
            return false;
        }
        posix_spawn_file_actions_t actions{};
        posix_spawnattr_t attributes{};
        int spawn_error = posix_spawn_file_actions_init(&actions);
        const bool actions_initialized = spawn_error == 0;
        bool attributes_initialized = false;
        if (spawn_error == 0) {
            spawn_error = posix_spawnattr_init(&attributes);
            attributes_initialized = spawn_error == 0;
        }
        if (spawn_error == 0) {
            spawn_error = posix_spawn_file_actions_adddup2(
                &actions, stdin_pipe[0], STDIN_FILENO);
        }
        if (spawn_error == 0) {
            spawn_error = posix_spawn_file_actions_adddup2(
                &actions, stdout_pipe[1], STDOUT_FILENO);
        }
        if (spawn_error == 0) {
            spawn_error = posix_spawn_file_actions_adddup2(
                &actions, stdout_pipe[1], STDERR_FILENO);
        }
        for (const int fd :
             {stdin_pipe[0], stdin_pipe[1], stdout_pipe[0], stdout_pipe[1]}) {
            if (spawn_error == 0 && fd != STDIN_FILENO && fd != STDOUT_FILENO &&
                fd != STDERR_FILENO) {
                spawn_error = posix_spawn_file_actions_addclose(&actions, fd);
            }
        }
        if (spawn_error == 0) {
            spawn_error =
                posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
        }
        if (spawn_error == 0) {
            spawn_error = posix_spawnattr_setpgroup(&attributes, 0);
        }

        auto environment_storage = childEnvironmentStorage();
        std::vector<char*> environment;
        environment.reserve(environment_storage.size() + 1);
        for (auto& entry : environment_storage) {
            environment.push_back(entry.data());
        }
        environment.push_back(nullptr);
        char bash[] = "bash";
        char no_rc[] = "--norc";
        char no_profile[] = "--noprofile";
        char interactive[] = "-i";
        char* argv[] = {bash, no_rc, no_profile, interactive, nullptr};
        pid_t child = -1;
        if (spawn_error == 0) {
            spawn_error = posix_spawnp(&child, bash, &actions, &attributes,
                                       argv, environment.data());
        }
        if (actions_initialized) {
            (void)posix_spawn_file_actions_destroy(&actions);
        }
        if (attributes_initialized) {
            (void)posix_spawnattr_destroy(&attributes);
        }
        if (spawn_error != 0) {
            close(stdin_pipe[0]);
            close(stdin_pipe[1]);
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);
            LOG(ERROR) << "Failed to spawn ibash: " << strerror(spawn_error);
            return false;
        }

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        (void)setpgid(child, child);
        if (!setNonBlocking(stdin_pipe[1]) || !setNonBlocking(stdout_pipe[0])) {
            close(stdin_pipe[1]);
            close(stdout_pipe[0]);
            (void)kill(-child, SIGKILL);
            (void)kill(child, SIGKILL);
            const auto reap_deadline = Clock::now() + kKillWait;
            while (Clock::now() < reap_deadline) {
                const auto result = waitpid(child, nullptr, WNOHANG);
                if (result == child || (result == -1 && errno == ECHILD)) {
                    break;
                }
                if (result == -1 && errno != EINTR) {
                    break;
                }
                std::this_thread::sleep_for(20ms);
            }
            return false;
        }

        pid_ = child;
        process_group_.store(child, std::memory_order_release);
        stdin_fd_ = stdin_pipe[1];
        stdout_fd_ = stdout_pipe[0];
        running_ = true;
        last_activity_ = Clock::now();
        termination_deadline_ms_.store(0, std::memory_order_release);
        return true;
    }

    bool isRunning() {
        std::scoped_lock lock(mutex_);
        return refreshRunningLocked();
    }

    bool isIdle(Clock::time_point now) {
        std::scoped_lock lock(mutex_);
        return !running_ || now - last_activity_ >= kIdleTimeout;
    }

    std::optional<std::string> execute(const std::string& command,
                                       const std::string& no_output,
                                       const std::string& output_truncated,
                                       std::stop_token stop,
                                       Clock::time_point deadline) {
        std::scoped_lock lock(mutex_);
        if (!refreshRunningLocked() || command.size() > kMaxCommandSize ||
            stop.stop_requested() || Clock::now() >= deadline) {
            return std::nullopt;
        }
        last_activity_ = Clock::now();

        const auto sequence = ++sequence_;
        const auto marker = fmt::format("\x1eGLIDER_IBASH_{}\x1f", sequence);
        const auto payload =
            command +
            fmt::format("\nprintf '\\036GLIDER_IBASH_{}\\037\\n'\n", sequence);
        if (!writeAllLocked(payload, stop, deadline)) {
            return std::nullopt;
        }
        auto output =
            readUntilMarkerLocked(marker, output_truncated, stop, deadline);
        if (!output) {
            return std::nullopt;
        }
        last_activity_ = Clock::now();
        if (output->empty()) {
            return no_output;
        }
        return output;
    }

    void requestTermination() noexcept {
        const pid_t pgid = process_group_.load(std::memory_order_acquire);
        if (pgid <= 0) {
            return;
        }
        std::int64_t expected = 0;
        const auto deadline =
            steadyMillis() +
            std::chrono::duration_cast<std::chrono::milliseconds>(
                kTerminateGrace)
                .count();
        (void)termination_deadline_ms_.compare_exchange_strong(
            expected, deadline, std::memory_order_acq_rel);
        if (kill(-pgid, SIGTERM) == -1 && errno == ESRCH) {
            (void)kill(pgid, SIGTERM);
        }
    }

    void cleanup() noexcept {
        requestTermination();
        std::scoped_lock lock(mutex_);

        if (stdin_fd_ != -1) {
            close(stdin_fd_);
            stdin_fd_ = -1;
        }
        if (stdout_fd_ != -1) {
            close(stdout_fd_);
            stdout_fd_ = -1;
        }

        const pid_t pgid = process_group_.load(std::memory_order_acquire);
        if (pgid <= 0) {
            running_ = false;
            pid_ = -1;
            return;
        }
        auto term_deadline =
            termination_deadline_ms_.load(std::memory_order_acquire);
        if (term_deadline == 0) {
            term_deadline = steadyMillis();
        }
        while (processGroupExists(pgid) && steadyMillis() < term_deadline) {
            reapLeaderLocked();
            std::this_thread::sleep_for(20ms);
        }
        if (processGroupExists(pgid)) {
            if (kill(-pgid, SIGKILL) == -1 && errno == ESRCH) {
                (void)kill(pgid, SIGKILL);
            }
        }

        const auto kill_deadline = Clock::now() + kKillWait;
        while (Clock::now() < kill_deadline) {
            reapLeaderLocked();
            if (!processGroupExists(pgid)) {
                break;
            }
            std::this_thread::sleep_for(20ms);
        }
        reapLeaderLocked();
        running_ = false;
        pid_ = -1;
        process_group_.store(-1, std::memory_order_release);
    }

   private:
    static bool processGroupExists(pid_t pgid) noexcept {
        if (kill(-pgid, 0) == 0) {
            return true;
        }
        return errno == EPERM;
    }

    void reapLeaderLocked() noexcept {
        if (pid_ <= 0) {
            return;
        }
        int status = 0;
        pid_t result;
        do {
            result = waitpid(pid_, &status, WNOHANG);
        } while (result == -1 && errno == EINTR);
        if (result == pid_ || (result == -1 && errno == ECHILD)) {
            pid_ = -1;
            running_ = false;
        }
    }

    bool refreshRunningLocked() {
        if (!running_ || pid_ <= 0) {
            return false;
        }
        int status = 0;
        pid_t result;
        do {
            result = waitpid(pid_, &status, WNOHANG);
        } while (result == -1 && errno == EINTR);
        if (result == 0) {
            return true;
        }
        if (result == pid_ || (result == -1 && errno == ECHILD)) {
            pid_ = -1;
            running_ = false;
        }
        return false;
    }

    bool writeAllLocked(const std::string& payload, std::stop_token stop,
                        Clock::time_point deadline) {
        std::size_t offset = 0;
        while (offset < payload.size()) {
            if (stop.stop_requested() ||
                termination_deadline_ms_.load(std::memory_order_acquire) != 0 ||
                Clock::now() >= deadline) {
                return false;
            }
            pollfd fd = {.fd = stdin_fd_, .events = POLLOUT, .revents = 0};
            int result;
            do {
                result = poll(&fd, 1, pollTimeout(deadline));
            } while (result == -1 && errno == EINTR);
            if (result <= 0 || (fd.revents & (POLLERR | POLLHUP | POLLNVAL))) {
                if (result == 0)
                    continue;
                return false;
            }
            const auto written = writeWithoutSigpipe(
                stdin_fd_, payload.data() + offset, payload.size() - offset);
            if (written > 0) {
                offset += static_cast<std::size_t>(written);
            } else if (written == -1 &&
                       (errno == EAGAIN || errno == EWOULDBLOCK)) {
                continue;
            } else {
                return false;
            }
        }
        return true;
    }

    static void appendLimited(std::string& output, std::string_view data,
                              bool& truncated) {
        const auto available =
            output.size() < kMaxOutputSize ? kMaxOutputSize - output.size() : 0;
        const auto copied = std::min(available, data.size());
        output.append(data.data(), copied);
        truncated = truncated || copied != data.size();
    }

    std::optional<std::string> readUntilMarkerLocked(
        const std::string& marker, const std::string& output_truncated,
        std::stop_token stop, Clock::time_point deadline) {
        std::string output;
        std::string carry;
        bool truncated = false;
        std::array<char, kReadBufferSize> buffer{};

        while (Clock::now() < deadline) {
            if (stop.stop_requested() ||
                termination_deadline_ms_.load(std::memory_order_acquire) != 0) {
                return std::nullopt;
            }
            pollfd fd = {.fd = stdout_fd_, .events = POLLIN, .revents = 0};
            int result;
            do {
                result = poll(&fd, 1, pollTimeout(deadline));
            } while (result == -1 && errno == EINTR);
            if (result == 0) {
                continue;
            }
            if (result == -1 || (fd.revents & POLLNVAL)) {
                return std::nullopt;
            }

            bool read_any = false;
            while (true) {
                if (stop.stop_requested() ||
                    termination_deadline_ms_.load(std::memory_order_acquire) !=
                        0 ||
                    Clock::now() >= deadline) {
                    return std::nullopt;
                }
                const auto bytes =
                    read(stdout_fd_, buffer.data(), buffer.size());
                if (bytes > 0) {
                    read_any = true;
                    std::string combined = carry;
                    combined.append(buffer.data(),
                                    static_cast<std::size_t>(bytes));
                    const auto marker_pos = combined.find(marker);
                    if (marker_pos != std::string::npos) {
                        if (stop.stop_requested() ||
                            termination_deadline_ms_.load(
                                std::memory_order_acquire) != 0 ||
                            Clock::now() >= deadline) {
                            return std::nullopt;
                        }
                        appendLimited(
                            output,
                            std::string_view(combined).substr(0, marker_pos),
                            truncated);
                        if (truncated) {
                            const auto suffix = "\n" + output_truncated;
                            if (suffix.size() >= kMaxOutputSize) {
                                output.assign(suffix, 0, kMaxOutputSize);
                            } else {
                                output.resize(
                                    std::min(output.size(),
                                             kMaxOutputSize - suffix.size()));
                                output += suffix;
                            }
                        }
                        return output;
                    }

                    const auto keep = std::min(
                        combined.size(),
                        marker.empty() ? std::size_t{0} : marker.size() - 1);
                    appendLimited(output,
                                  std::string_view(combined).substr(
                                      0, combined.size() - keep),
                                  truncated);
                    carry.assign(combined, combined.size() - keep, keep);
                    continue;
                }
                if (bytes == -1 && errno == EINTR) {
                    continue;
                }
                if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    break;
                }
                return std::nullopt;
            }
            if (!read_any && (fd.revents & (POLLERR | POLLHUP))) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    std::mutex mutex_;
    pid_t pid_ = -1;
    std::atomic<pid_t> process_group_{-1};
    std::atomic<std::int64_t> termination_deadline_ms_{0};
    int stdin_fd_ = -1;
    int stdout_fd_ = -1;
    bool running_ = false;
    std::uint64_t sequence_ = 0;
    Clock::time_point last_activity_{};
};

class InteractiveBashManager {
   public:
    enum class ExecuteStatus { Success, NoSession, Failed };
    struct ExecuteResult {
        ExecuteStatus status = ExecuteStatus::Failed;
        std::string output;
    };

    static InteractiveBashManager& getInstance() {
        static InteractiveBashManager instance;
        return instance;
    }

    static InteractiveBashManager* existingInstance() noexcept {
        return existing_.load(std::memory_order_acquire);
    }

    bool startSession(const SessionKey& key, std::stop_token stop,
                      Clock::time_point deadline) {
        if (!ensureReaper()) {
            return false;
        }
        std::shared_ptr<BashSession> existing;
        {
            std::scoped_lock lock(mutex_);
            const auto found = sessions_.find(key);
            if (found != sessions_.end()) {
                existing = found->second;
            }
            if (!existing && sessions_.size() >= kMaxSessions) {
                return false;
            }
        }
        if (existing) {
            if (existing->isRunning()) {
                return false;
            }
            retireSession(key, existing);
        }

        auto session = std::make_shared<BashSession>();
        if (!session->start(stop, deadline)) {
            return false;
        }
        if (stop.stop_requested() || Clock::now() >= deadline) {
            std::scoped_lock lock(mutex_);
            retired_.push_back(session);
            session->requestTermination();
            reaper_cv_.notify_all();
            return false;
        }
        {
            std::scoped_lock lock(mutex_);
            if (sessions_.size() >= kMaxSessions || sessions_.contains(key)) {
                retired_.push_back(session);
                session->requestTermination();
                reaper_cv_.notify_all();
                return false;
            }
            sessions_.emplace(key, session);
        }
        return true;
    }

    ExecuteResult executeCommand(const SessionKey& key,
                                 const std::string& command,
                                 const std::string& no_output,
                                 const std::string& output_truncated,
                                 std::stop_token stop,
                                 Clock::time_point deadline) {
        std::shared_ptr<BashSession> session;
        {
            std::scoped_lock lock(mutex_);
            const auto found = sessions_.find(key);
            if (found == sessions_.end()) {
                return {.status = ExecuteStatus::NoSession};
            }
            session = found->second;
        }
        if (session->isIdle(Clock::now()) || !session->isRunning()) {
            retireSession(key, session);
            return {.status = ExecuteStatus::NoSession};
        }
        auto output = session->execute(command, no_output, output_truncated,
                                       stop, deadline);
        if (!output) {
            retireSession(key, session);
            return {.status = ExecuteStatus::Failed};
        }
        return {.status = ExecuteStatus::Success, .output = std::move(*output)};
    }

    bool retireSession(const SessionKey& key) {
        std::shared_ptr<BashSession> session;
        {
            std::scoped_lock lock(mutex_);
            const auto found = sessions_.find(key);
            if (found == sessions_.end()) {
                return false;
            }
            session = found->second;
            sessions_.erase(found);
            retired_.push_back(session);
        }
        session->requestTermination();
        reaper_cv_.notify_all();
        return true;
    }

    void shutdown() {
        std::scoped_lock lifecycle_lock(lifecycle_mutex_);
        if (reaper_.joinable()) {
            reaper_.request_stop();
            reaper_cv_.notify_all();
            reaper_.join();
        }
        shutdownSessions();
    }

   private:
    bool ensureReaper() {
        try {
            std::scoped_lock lifecycle_lock(lifecycle_mutex_);
            if (!reaper_.joinable()) {
                reaper_ = std::jthread(
                    [this](std::stop_token stop) { reaperLoop(stop); });
            }
            return true;
        } catch (const std::exception& error) {
            LOG(ERROR) << "Failed to start ibash session reaper: "
                       << error.what();
        } catch (...) {
            LOG(ERROR) << "Failed to start ibash session reaper";
        }
        return false;
    }

    void shutdownSessions() {
        std::scoped_lock cleanup_lock(cleanup_mutex_);
        std::map<SessionKey, std::shared_ptr<BashSession>> sessions;
        std::vector<std::shared_ptr<BashSession>> retired;
        {
            std::scoped_lock lock(mutex_);
            sessions.swap(sessions_);
            retired.swap(retired_);
        }
        for (const auto& entry : sessions) {
            entry.second->requestTermination();
        }
        for (const auto& session : retired) {
            session->requestTermination();
        }
        for (const auto& entry : sessions) {
            entry.second->cleanup();
        }
        for (const auto& session : retired) {
            session->cleanup();
        }
    }

    InteractiveBashManager() {
        existing_.store(this, std::memory_order_release);
    }

    ~InteractiveBashManager() {
        shutdown();
        existing_.store(nullptr, std::memory_order_release);
    }

    void retireSession(const SessionKey& key,
                       const std::shared_ptr<BashSession>& expected) {
        bool retired = false;
        {
            std::scoped_lock lock(mutex_);
            const auto found = sessions_.find(key);
            if (found != sessions_.end() && found->second == expected) {
                retired_.push_back(found->second);
                sessions_.erase(found);
                retired = true;
            }
        }
        if (retired) {
            expected->requestTermination();
            reaper_cv_.notify_all();
        }
    }

    void expireIdle() {
        std::vector<std::pair<SessionKey, std::shared_ptr<BashSession>>>
            snapshot;
        {
            std::scoped_lock lock(mutex_);
            for (const auto& entry : sessions_) {
                snapshot.push_back(entry);
            }
        }
        const auto now = Clock::now();
        for (const auto& [key, session] : snapshot) {
            if (session->isIdle(now) || !session->isRunning()) {
                retireSession(key, session);
            }
        }
    }

    void cleanupRetired() {
        std::scoped_lock cleanup_lock(cleanup_mutex_);
        std::vector<std::shared_ptr<BashSession>> retired;
        {
            std::scoped_lock lock(mutex_);
            retired.swap(retired_);
        }
        for (const auto& session : retired) {
            session->requestTermination();
        }
        for (const auto& session : retired) {
            session->cleanup();
        }
    }

    void reaperLoop(std::stop_token stop) {
        std::unique_lock wait_lock(reaper_mutex_);
        while (!stop.stop_requested()) {
            reaper_cv_.wait_for(wait_lock, kReaperInterval);
            if (stop.stop_requested()) {
                break;
            }
            wait_lock.unlock();
            expireIdle();
            cleanupRetired();
            wait_lock.lock();
        }
    }

    InteractiveBashManager(const InteractiveBashManager&) = delete;
    InteractiveBashManager& operator=(const InteractiveBashManager&) = delete;

    std::mutex mutex_;
    std::map<SessionKey, std::shared_ptr<BashSession>> sessions_;
    std::vector<std::shared_ptr<BashSession>> retired_;
    std::mutex lifecycle_mutex_;
    std::mutex cleanup_mutex_;
    std::mutex reaper_mutex_;
    std::condition_variable reaper_cv_;
    std::jthread reaper_;
    static inline std::atomic<InteractiveBashManager*> existing_{nullptr};
};

std::optional<SessionKey> sessionKey(const MessageExt& message) {
    if (!message.has<MessageAttrs::User>()) {
        return std::nullopt;
    }
    return SessionKey{.chat = message.get<MessageAttrs::Chat>()->id,
                      .user = message.get<MessageAttrs::User>()->id};
}

void ibashWork(TgBotApi::Ptr api, MessageExt::Ptr message,
               const StringResLoader::PerLocaleMap* res, std::stop_token stop,
               Clock::time_point deadline) {
    const auto key = sessionKey(*message);
    if (!key || stop.stop_requested() || Clock::now() >= deadline) {
        return;
    }
    auto& manager = InteractiveBashManager::getInstance();

    if (!message->has<MessageAttrs::ExtraText>()) {
        api->sendReplyMessage(
            message->message(),
            manager.startSession(*key, stop, deadline)
                ? res->get(Strings::IBASH_SESSION_STARTED)
                : res->get(Strings::IBASH_SESSION_START_FAILED));
        return;
    }

    const auto result = manager.executeCommand(
        *key, message->get<MessageAttrs::ExtraText>(),
        std::string(res->get(Strings::IBASH_NO_OUTPUT)),
        std::string(res->get(Strings::IBASH_OUTPUT_TRUNCATED)), stop, deadline);
    switch (result.status) {
        case InteractiveBashManager::ExecuteStatus::Success:
            api->sendReplyMessage(message->message(), result.output);
            break;
        case InteractiveBashManager::ExecuteStatus::NoSession:
            api->sendReplyMessage(message->message(),
                                  res->get(Strings::IBASH_START_FIRST));
            break;
        case InteractiveBashManager::ExecuteStatus::Failed:
            api->sendReplyMessage(message->message(),
                                  res->get(Strings::IBASH_EXEC_FAILED));
            break;
    }
}

DECLARE_COMMAND_HANDLER(ibash) {
    const auto key = sessionKey(*message);
    if (!key) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::IBASH_EXEC_FAILED));
        return;
    }

    if (message->has<MessageAttrs::ExtraText>()) {
        // MessageExt::get<ExtraText>() returns an owning string. Keep it alive
        // while the trimmed view is inspected; binding trimAscii() directly to
        // the temporary leaves control dangling at the end of the initializer.
        const auto extra_text = message->get<MessageAttrs::ExtraText>();
        const auto control = trimAscii(extra_text);
        if (control == "exit" || control == "cancel") {
            const bool ended =
                InteractiveBashManager::getInstance().retireSession(*key);
            api->sendReplyMessage(
                message->message(),
                ended ? res->get(Strings::IBASH_SESSION_ENDED)
                      : res->get(Strings::IBASH_NO_ACTIVE_SESSION));
            return;
        }
    }

    auto owned_message = std::make_shared<MessageExt>(*message);
    const auto deadline = Clock::now() + kCommandDeadline;
    if (!api->submitCommandWork(
            "ibash", TgBotApi::WorkClass::Process,
            [api, res, owned_message = std::move(owned_message),
             deadline](std::stop_token stop) {
                ibashWork(api, owned_message.get(), res, stop, deadline);
            },
            {.deadline = kCommandDeadline})) {
        api->sendReplyMessage(message->message(),
                              "The process queue is full. Please retry later.");
    }
}

}  // namespace

extern "C" DYN_COMMAND_EXPORT void DYN_COMMAND_CLEANUP_SYM() noexcept {
    try {
        if (auto* manager = InteractiveBashManager::existingInstance()) {
            manager->shutdown();
        }
    } catch (const std::exception& error) {
        LOG(ERROR) << "Failed to clean up ibash sessions: " << error.what();
    } catch (...) {
        LOG(ERROR) << "Failed to clean up ibash sessions";
    }
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::OwnerOnly,
    .name = "ibash",
    .description = "Interactive bash shell",
    .function = COMMAND_HANDLER_NAME(ibash),
};
