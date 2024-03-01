#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

class ThreadPool {
   public:
    explicit ThreadPool(size_t threadCount) {
        for (size_t i = 0; i < threadCount; ++i) {
            threads.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mutex);
                        condition.wait(lock, [this] { return !tasks.empty() || stop; });
                        if (stop && tasks.empty())
                            return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    bool isRunning() {
        bool ret;
        {
            std::unique_lock<std::mutex> _(mutex);
            ret = !stop;
        }
        return ret;
    }

    void Shutdown() {
        {
            std::unique_lock<std::mutex> _(mutex);
            if (stop)
                return;
            stop = true;
        }
        condition.notify_all();
        for (auto &thread : threads)
            thread.join();
    }

    template <typename Func, typename... Args>
    void Enqueue(Func &&func, Args &&...args) {
        {
            std::unique_lock<std::mutex> _(mutex);
            if (stop)
                return;
            tasks.emplace([=] { std::invoke(func, args...); });
        }
        condition.notify_one();
    }

    ~ThreadPool() { Shutdown(); }

   private:
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;
    std::mutex mutex;
    std::condition_variable condition;
    bool stop = false;
};
