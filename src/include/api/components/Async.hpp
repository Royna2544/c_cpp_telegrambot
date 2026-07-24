#pragma once

#include <api/TgBotApiImpl.hpp>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <utility>
#include <vector>

class TgBotApiImpl::Async {
    // A flag to stop CallbackQuery worker
    std::atomic<bool> stopWorker = false;
    // A bounded queue of work consumed by the fixed worker set.
    std::queue<std::pair<std::string, std::function<void()>>> tasks;
    std::size_t maxQueueSize;
    // mutex to protect shared queue
    std::mutex mutex;
    // condition variable to wait for async tasks to finish.
    std::condition_variable condVariable;
    // worker thread(s) to consume command queue
    std::vector<std::thread> threads;
    // name, for logging purposes
    std::string _name;

    void threadFunction();

   public:
    explicit Async(std::string name, int count,
                   std::size_t maxQueueSize = 32);
    ~Async();

    NO_COPY_CTOR(Async);
    [[nodiscard]] bool emplaceTask(std::string command,
                                   std::function<void()> task);
};