#include <api/TgBotApiImpl.hpp>
#include <api/components/Async.hpp>

#include <stdexcept>

bool TgBotApiImpl::Async::emplaceTask(std::string command,
                                      std::function<void()> task) {
    std::unique_lock<std::mutex> lock(mutex);
    if (stopWorker || tasks.size() >= maxQueueSize) {
        return false;
    }
    tasks.emplace(std::move(command), std::move(task));
    lock.unlock();
    condVariable.notify_one();
    return true;
}

TgBotApiImpl::Async::Async(std::string name, const int count,
                           const std::size_t maxQueueSize)
    : maxQueueSize(maxQueueSize), _name(std::move(name)) {
    if (count <= 0 || maxQueueSize == 0) {
        throw std::invalid_argument(
            "Async requires at least one worker and one queue slot");
    }
    DLOG(INFO) << fmt::format("Starting AsyncThreads '{}', count: {}", _name,
                              count);
    for (int i = 0; i < count; ++i) {
        threads.emplace_back([this]() { threadFunction(); });
    }
}

TgBotApiImpl::Async::~Async() {
    DLOG(INFO) << fmt::format("Stopping AsyncThreads '{}'", _name);
    stopWorker = true;
    condVariable.notify_all();
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads.clear();
}

void TgBotApiImpl::Async::threadFunction() {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex);
        condVariable.wait(lock,
                          [this] { return !tasks.empty() || stopWorker; });
        if (tasks.empty()) {
            if (stopWorker) {
                return;
            }
            continue;
        }

        auto front = std::move(tasks.front());
        tasks.pop();
        lock.unlock();
        try {
            front.second();
        } catch (const TgBot::TgException& e) {
            LOG(ERROR) << fmt::format(
                "[AsyncConsumer] While handling command: {}: TgApi Exception: {}",
                front.first, e.what());
        } catch (const std::exception& e) {
            LOG(ERROR) << fmt::format(
                "[AsyncConsumer] While handling command: {}: Exception: {}",
                front.first, e.what());
        } catch (...) {
            LOG(ERROR) << fmt::format(
                "[AsyncConsumer] While handling command: {}: Unknown exception",
                front.first);
        }
    }
}
