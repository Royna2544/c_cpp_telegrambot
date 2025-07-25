#include <api/TgBotApiImpl.hpp>
#include <api/components/Async.hpp>

void TgBotApiImpl::Async::emplaceTask(std::string command,
                                      std::future<void> future) {
    std::unique_lock<std::mutex> lock(mutex);
    tasks.emplace(std::move(command), std::move(future));
    condVariable.notify_one();
}

TgBotApiImpl::Async::Async(std::string name, const int count)
    : _name(std::move(name)) {
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
    while (!stopWorker) {
        std::unique_lock<std::mutex> lock(mutex);
        condVariable.wait(lock,
                          [this] { return !tasks.empty() || stopWorker; });

        if (!tasks.empty()) {
            auto front = std::move(tasks.front());
            tasks.pop();
            lock.unlock();
            try {
                // Wait for the task to complete
                front.second.get();
            } catch (const TgBot::TgException& e) {
                LOG(ERROR) << fmt::format(
                    "[AsyncConsumer] While handling command: {}: TgApi "
                    "Exception: {}",
                    front.first, e.what());
            } catch (const std::exception& e) {
                LOG(ERROR) << fmt::format(
                    "[AsyncConsumer] While handling command: {}: Exception: {}",
                    front.first, e.what());
            } catch (...) {
            }
        }
    }
}
