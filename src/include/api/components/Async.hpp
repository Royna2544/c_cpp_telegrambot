#pragma once

#include <api/TgBotApiImpl.hpp>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

class TgBotApiImpl::Async {
    using Task = std::pair<std::string, std::function<void()>>;

    // A flag to stop CallbackQuery worker
    std::atomic<bool> stopWorker = false;
    // A bounded queue of work consumed by the fixed worker set.
    std::queue<Task> tasks;
    std::size_t maxQueueSize;
    // Running tasks are tracked by owner so module unload can wait until every
    // callable for that owner has returned and been destroyed.
    std::unordered_map<std::string, std::size_t> activeTasks;
    // A positive count rejects new work for an owner between cancel() and the
    // matching drain(). Counts make concurrent cancellation scopes safe.
    std::unordered_map<std::string, std::size_t> blockedOwners;
    // mutex to protect shared queue
    std::mutex mutex;
    // condition variable to wait for async tasks to finish.
    std::condition_variable condVariable;
    std::condition_variable drainedVariable;
    // worker thread(s) to consume command queue
    std::vector<std::thread> threads;
    // name, for logging purposes
    std::string _name;

    void threadFunction();

   public:
    enum class EnqueueResult { Accepted, QueueFullOrStopping, Rejected };

    explicit Async(std::string name, int count, std::size_t maxQueueSize = 32);
    ~Async();

    NO_COPY_CTOR(Async);
    [[nodiscard]] bool emplaceTask(std::string command,
                                   std::function<void()> task);
    [[nodiscard]] EnqueueResult emplaceTaskIf(
        std::string command, std::function<void()> task,
        const std::function<bool()>& admission);

    // Reject new tasks for owner and synchronously destroy all of its queued
    // callables. cancel() and drain() are split so callers can request
    // cancellation across other executors before waiting on active work.
    void cancel(std::string_view owner);
    // Wait for active owner tasks to return and destroy their callable, then
    // allow submissions for that owner again. When called by the active owner
    // task itself, waits for every *other* invocation and leaves the current
    // frame to unwind naturally rather than deadlocking on itself.
    void drain(std::string_view owner);
    void cancelAndDrain(std::string_view owner);
};
