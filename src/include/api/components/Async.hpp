#pragma once

#include <api/TgBotApiImpl.hpp>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>
#include <string>
#include <future>

class TgBotApiImpl::Async {
    // A flag to stop CallbackQuery worker
    std::atomic<bool> stopWorker = false;
    // A queue to handle command (commandname, async future)
    std::queue<std::pair<std::string, std::future<void>>> tasks;
    // mutex to protect shared queue
    std::mutex mutex;
    // condition variable to wait for async tasks to finish.
    std::condition_variable condVariable;
    // worker thread(s) to consume command queue
    std::vector<std::thread> threads;

    void threadFunction();

   public:
    explicit Async(const int count);
    ~Async();

    NO_COPY_CTOR(Async);
    void emplaceTask(std::string command, std::future<void> future);
};