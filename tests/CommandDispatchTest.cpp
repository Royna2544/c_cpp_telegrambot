#include <gtest/gtest.h>

#include <api/components/Async.hpp>

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>

using namespace std::chrono_literals;

TEST(CommandDispatchTest, RejectsWorkWhenBoundedQueueIsFull) {
    TgBotApiImpl::Async executor("test", 1, 1);
    std::mutex mutex;
    std::condition_variable started;
    std::condition_variable release;
    bool firstStarted = false;
    bool releaseFirst = false;

    ASSERT_TRUE(executor.emplaceTask("first", [&] {
        {
            const std::lock_guard lock(mutex);
            firstStarted = true;
        }
        started.notify_one();
        std::unique_lock lock(mutex);
        release.wait(lock, [&] { return releaseFirst; });
    }));

    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(started.wait_for(lock, 2s, [&] { return firstStarted; }));
    }

    ASSERT_TRUE(executor.emplaceTask("queued", [] {}));
    EXPECT_FALSE(executor.emplaceTask("overflow", [] {}));

    {
        const std::lock_guard lock(mutex);
        releaseFirst = true;
    }
    release.notify_one();
}
