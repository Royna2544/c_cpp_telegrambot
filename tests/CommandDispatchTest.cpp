#include <gtest/gtest.h>

#include <algorithm>
#include <api/components/Async.hpp>
#include <api/components/ModuleExecutionContext.hpp>
#include <api/components/OnAnyMessage.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

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

TEST(CommandDispatchTest, CommitsAdmissionOnlyWhenQueueHasCapacity) {
    TgBotApiImpl::Async executor("admission", 1, 1);
    std::mutex mutex;
    std::condition_variable started;
    std::condition_variable release;
    bool firstStarted = false;
    bool releaseFirst = false;

    ASSERT_TRUE(executor.emplaceTask("running", [&] {
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

    int committed = 0;
    EXPECT_EQ(executor.emplaceTaskIf(
                  "queued", [] {},
                  [&] {
                      ++committed;
                      return true;
                  }),
              TgBotApiImpl::Async::EnqueueResult::Accepted);
    EXPECT_EQ(executor.emplaceTaskIf(
                  "full", [] {},
                  [&] {
                      ++committed;
                      return true;
                  }),
              TgBotApiImpl::Async::EnqueueResult::QueueFullOrStopping);
    EXPECT_EQ(committed, 1);

    {
        const std::lock_guard lock(mutex);
        releaseFirst = true;
    }
    release.notify_one();
}

TEST(CommandDispatchTest,
     OwnerCancellationDestroysQueuedClosuresAndRejectsLateEnqueue) {
    struct LifetimeProbe {
        explicit LifetimeProbe(std::atomic<bool>* destroyed)
            : destroyed(destroyed) {}
        ~LifetimeProbe() { *destroyed = true; }
        std::atomic<bool>* destroyed;
    };

    TgBotApiImpl::Async executor("owner-cancel", 1, 8);
    std::mutex mutex;
    std::condition_variable changed;
    bool activeStarted = false;
    bool releaseActive = false;
    bool otherOwnerRan = false;
    std::atomic<bool> queuedRan = false;

    ASSERT_TRUE(executor.emplaceTask("target", [&] {
        std::unique_lock lock(mutex);
        activeStarted = true;
        changed.notify_all();
        (void)changed.wait_for(lock, 2s, [&] { return releaseActive; });
    }));
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(changed.wait_for(lock, 2s, [&] { return activeStarted; }));
    }

    std::atomic<bool> queuedDestroyed = false;
    auto queuedProbe = std::make_shared<LifetimeProbe>(&queuedDestroyed);
    const std::weak_ptr<LifetimeProbe> queuedWeak = queuedProbe;
    ASSERT_TRUE(executor.emplaceTask(
        "target", [probe = std::move(queuedProbe), &queuedRan] {
            (void)probe;
            queuedRan = true;
        }));
    ASSERT_TRUE(executor.emplaceTask("other", [&] {
        {
            const std::lock_guard lock(mutex);
            otherOwnerRan = true;
        }
        changed.notify_all();
    }));

    executor.cancel("target");
    EXPECT_TRUE(queuedDestroyed.load());
    EXPECT_TRUE(queuedWeak.expired());
    EXPECT_FALSE(queuedRan.load());

    // Model a delivery that acquired a module lease immediately before the
    // unload fence but reaches the command queue while its owner is blocked.
    // Its rejected closure must release that lease synchronously.
    std::atomic<bool> lateDestroyed = false;
    auto lateProbe = std::make_shared<LifetimeProbe>(&lateDestroyed);
    const std::weak_ptr<LifetimeProbe> lateWeak = lateProbe;
    auto lateTask = [probe = std::move(lateProbe)] { (void)probe; };
    EXPECT_FALSE(executor.emplaceTask("target", std::move(lateTask)));
    EXPECT_TRUE(lateDestroyed.load());
    EXPECT_TRUE(lateWeak.expired());

    std::atomic<bool> drainStarted = false;
    std::atomic<bool> drainReturned = false;
    std::thread drainer([&] {
        drainStarted = true;
        executor.drain("target");
        drainReturned = true;
        changed.notify_all();
    });
    while (!drainStarted.load()) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(drainReturned.load());

    {
        const std::lock_guard lock(mutex);
        releaseActive = true;
    }
    changed.notify_all();
    drainer.join();

    EXPECT_TRUE(drainReturned.load());
    EXPECT_TRUE(executor.emplaceTask("target", [] {}));
    {
        std::unique_lock lock(mutex);
        EXPECT_TRUE(changed.wait_for(lock, 2s, [&] { return otherOwnerRan; }));
    }
    EXPECT_FALSE(queuedRan.load());
}

TEST(CommandDispatchTest, ReentrantOwnerCancellationDoesNotDeadlock) {
    TgBotApiImpl::Async executor("self-cancel", 1, 4);
    std::mutex mutex;
    std::condition_variable changed;
    bool activeStarted = false;
    bool beginCancellation = false;
    bool cancellationReturned = false;
    std::atomic<bool> queuedRan = false;

    ASSERT_TRUE(executor.emplaceTask("self", [&] {
        {
            std::unique_lock lock(mutex);
            activeStarted = true;
            changed.notify_all();
            (void)changed.wait_for(lock, 2s, [&] { return beginCancellation; });
        }
        executor.cancelAndDrain("self");
        {
            const std::lock_guard lock(mutex);
            cancellationReturned = true;
        }
        changed.notify_all();
    }));
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(changed.wait_for(lock, 2s, [&] { return activeStarted; }));
    }
    ASSERT_TRUE(executor.emplaceTask("self", [&] { queuedRan = true; }));

    {
        const std::lock_guard lock(mutex);
        beginCancellation = true;
    }
    changed.notify_all();
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(
            changed.wait_for(lock, 2s, [&] { return cancellationReturned; }));
    }
    EXPECT_FALSE(queuedRan.load());
}

TEST(AnyMessageCallbackDispatcherTest,
     BoundsPendingWorkAndSerializesEachCallback) {
    tgbot::detail::AnyMessageCallbackDispatcher dispatcher(nullptr, 2, 1);
    std::mutex mutex;
    std::condition_variable changed;
    bool releaseFirst = false;
    bool sawExecutionScope = false;
    int started = 0;
    int completed = 0;
    int active = 0;
    int maxActive = 0;

    ASSERT_TRUE(dispatcher.registerCallback(
        "serial-owner", [&](TgBotApi::CPtr, const Message::Ptr&) {
            std::unique_lock lock(mutex);
            const bool first = started == 0;
            ++started;
            ++active;
            maxActive = std::max(maxActive, active);
            sawExecutionScope = sawExecutionScope ||
                                module_execution::isExecuting("serial-owner");
            changed.notify_all();
            if (first) {
                changed.wait(lock, [&] { return releaseFirst; });
            }
            --active;
            ++completed;
            changed.notify_all();
            return TgBotApi::AnyMessageResult::Handled;
        }));

    ASSERT_TRUE(dispatcher.enqueue(std::make_shared<Message>()));
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(changed.wait_for(lock, 2s, [&] { return started == 1; }));
    }

    // One invocation is running and exactly one can wait in the bounded
    // pending budget. The third message is rejected without partial enqueue.
    EXPECT_TRUE(dispatcher.enqueue(std::make_shared<Message>()));
    EXPECT_FALSE(dispatcher.enqueue(std::make_shared<Message>()));

    {
        const std::lock_guard lock(mutex);
        releaseFirst = true;
    }
    changed.notify_all();

    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(changed.wait_for(lock, 2s, [&] { return completed == 2; }));
        EXPECT_EQ(maxActive, 1);
        EXPECT_TRUE(sawExecutionScope);
    }
    EXPECT_FALSE(module_execution::isExecuting("serial-owner"));
}

TEST(AnyMessageCallbackDispatcherTest,
     OwnerRemovalCancelsQueuedWorkAndDrainsClosureLifetime) {
    struct LifetimeProbe {
        explicit LifetimeProbe(std::atomic<bool>* destroyed)
            : destroyed(destroyed) {}
        ~LifetimeProbe() { *destroyed = true; }
        std::atomic<bool>* destroyed;
    };

    tgbot::detail::AnyMessageCallbackDispatcher dispatcher(nullptr, 2, 4);
    std::mutex mutex;
    std::condition_variable changed;
    bool callbackStarted = false;
    bool releaseCallback = false;
    int callbackCalls = 0;
    std::atomic<bool> destroyed = false;
    auto probe = std::make_shared<LifetimeProbe>(&destroyed);
    const std::weak_ptr<LifetimeProbe> weakProbe = probe;

    ASSERT_TRUE(dispatcher.registerCallback(
        "owned", [probe, &mutex, &changed, &callbackStarted, &releaseCallback,
                  &callbackCalls](TgBotApi::CPtr, const Message::Ptr&) {
            std::unique_lock lock(mutex);
            ++callbackCalls;
            callbackStarted = true;
            changed.notify_all();
            changed.wait(lock, [&] { return releaseCallback; });
            return TgBotApi::AnyMessageResult::Handled;
        }));
    probe.reset();

    ASSERT_TRUE(dispatcher.enqueue(std::make_shared<Message>()));
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(
            changed.wait_for(lock, 2s, [&] { return callbackStarted; }));
    }
    ASSERT_TRUE(dispatcher.enqueue(std::make_shared<Message>()));

    std::atomic<bool> removalStarted = false;
    std::atomic<bool> removalReturned = false;
    std::thread remover([&] {
        removalStarted = true;
        dispatcher.removeCallbacksForCommand("owned");
        removalReturned = true;
    });
    while (!removalStarted.load()) {
        std::this_thread::yield();
    }
    EXPECT_FALSE(removalReturned.load());
    EXPECT_FALSE(destroyed.load());

    {
        const std::lock_guard lock(mutex);
        releaseCallback = true;
    }
    changed.notify_all();
    remover.join();

    EXPECT_TRUE(removalReturned.load());
    EXPECT_TRUE(destroyed.load());
    EXPECT_TRUE(weakProbe.expired());
    EXPECT_EQ(callbackCalls, 1);
}

TEST(AnyMessageCallbackDispatcherTest,
     GlobalSubscriptionCancellationDrainsActiveCallback) {
    struct LifetimeProbe {
        explicit LifetimeProbe(std::atomic<bool>* destroyed)
            : destroyed(destroyed) {}
        ~LifetimeProbe() { *destroyed = true; }
        std::atomic<bool>* destroyed;
    };

    tgbot::detail::AnyMessageCallbackDispatcher dispatcher(nullptr, 1, 4);
    std::mutex mutex;
    std::condition_variable changed;
    bool callbackStarted = false;
    bool releaseCallback = false;
    int callbackCalls = 0;
    std::atomic<bool> destroyed = false;
    auto probe = std::make_shared<LifetimeProbe>(&destroyed);
    const std::weak_ptr<LifetimeProbe> weakProbe = probe;

    auto subscription = dispatcher.subscribeCallback(
        [probe, &mutex, &changed, &callbackStarted, &releaseCallback,
         &callbackCalls](TgBotApi::CPtr, const Message::Ptr&) {
            std::unique_lock lock(mutex);
            ++callbackCalls;
            callbackStarted = true;
            changed.notify_all();
            changed.wait(lock, [&] { return releaseCallback; });
            return TgBotApi::AnyMessageResult::Handled;
        });
    ASSERT_NE(subscription, nullptr);
    probe.reset();

    ASSERT_TRUE(dispatcher.enqueue(std::make_shared<Message>()));
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(
            changed.wait_for(lock, 2s, [&] { return callbackStarted; }));
    }

    std::atomic<bool> cancellationStarted = false;
    std::atomic<bool> cancellationReturned = false;
    std::thread canceller([subscription = std::move(subscription),
                           &cancellationStarted,
                           &cancellationReturned]() mutable {
        cancellationStarted = true;
        subscription.reset();
        cancellationReturned = true;
    });
    while (!cancellationStarted.load()) {
        std::this_thread::yield();
    }
    EXPECT_FALSE(cancellationReturned.load());
    EXPECT_FALSE(destroyed.load());

    {
        const std::lock_guard lock(mutex);
        releaseCallback = true;
    }
    changed.notify_all();
    canceller.join();

    EXPECT_TRUE(cancellationReturned.load());
    EXPECT_TRUE(destroyed.load());
    EXPECT_TRUE(weakProbe.expired());
    dispatcher.removeCallbacksForCommand("");
    EXPECT_EQ(callbackCalls, 1);
}

TEST(AnyMessageCallbackDispatcherTest,
     GlobalSubscriptionSafelyOutlivesDispatcher) {
    std::atomic<bool> destroyed = false;
    struct LifetimeProbe {
        explicit LifetimeProbe(std::atomic<bool>* destroyed)
            : destroyed(destroyed) {}
        ~LifetimeProbe() { *destroyed = true; }
        std::atomic<bool>* destroyed;
    };

    TgBotApi::CallbackSubscription::Ptr subscription;
    {
        tgbot::detail::AnyMessageCallbackDispatcher dispatcher(nullptr, 1, 4);
        auto probe = std::make_shared<LifetimeProbe>(&destroyed);
        subscription = dispatcher.subscribeCallback(
            [probe](TgBotApi::CPtr, const Message::Ptr&) {
                return TgBotApi::AnyMessageResult::Handled;
            });
        ASSERT_NE(subscription, nullptr);
        probe.reset();
        EXPECT_FALSE(destroyed.load());
    }

    EXPECT_FALSE(destroyed.load());
    subscription.reset();
    EXPECT_TRUE(destroyed.load());
}

TEST(AnyMessageCallbackDispatcherTest,
     ExceptionDeregistersOnlyFailingCallback) {
    tgbot::detail::AnyMessageCallbackDispatcher dispatcher(nullptr, 2, 8);
    std::mutex mutex;
    std::condition_variable changed;
    int failingCalls = 0;
    int healthyCalls = 0;

    ASSERT_TRUE(dispatcher.registerCallback(
        "failing",
        [&](TgBotApi::CPtr, const Message::Ptr&) -> TgBotApi::AnyMessageResult {
            {
                const std::lock_guard lock(mutex);
                ++failingCalls;
            }
            changed.notify_all();
            throw std::runtime_error("expected test failure");
        }));
    ASSERT_TRUE(dispatcher.registerCallback(
        "healthy", [&](TgBotApi::CPtr, const Message::Ptr&) {
            {
                const std::lock_guard lock(mutex);
                ++healthyCalls;
            }
            changed.notify_all();
            return TgBotApi::AnyMessageResult::Handled;
        }));

    ASSERT_TRUE(dispatcher.enqueue(std::make_shared<Message>()));
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(changed.wait_for(
            lock, 2s, [&] { return failingCalls == 1 && healthyCalls == 1; }));
    }
    // Also drains the race between the throw and automatic deregistration.
    dispatcher.removeCallbacksForCommand("failing");

    ASSERT_TRUE(dispatcher.enqueue(std::make_shared<Message>()));
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(
            changed.wait_for(lock, 2s, [&] { return healthyCalls == 2; }));
        EXPECT_EQ(failingCalls, 1);
    }
}

TEST(AnyMessageCallbackDispatcherTest,
     ReentrantOwnerCancellationDoesNotDeadlockOrReenter) {
    tgbot::detail::AnyMessageCallbackDispatcher dispatcher(nullptr, 2, 4);
    std::mutex mutex;
    std::condition_variable changed;
    int calls = 0;

    ASSERT_TRUE(dispatcher.registerCallback(
        "self-cancelling", [&](TgBotApi::CPtr, const Message::Ptr&) {
            dispatcher.removeCallbacksForCommand("self-cancelling");
            {
                const std::lock_guard lock(mutex);
                ++calls;
            }
            changed.notify_all();
            return TgBotApi::AnyMessageResult::Handled;
        }));

    ASSERT_TRUE(dispatcher.enqueue(std::make_shared<Message>()));
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(changed.wait_for(lock, 2s, [&] { return calls == 1; }));
    }
    ASSERT_TRUE(dispatcher.enqueue(std::make_shared<Message>()));
    std::this_thread::yield();
    EXPECT_EQ(calls, 1);
}

TEST(AnyMessageCallbackDispatcherTest,
     ReloadedOwnerDoesNotReceiveMessagesQueuedForOldGeneration) {
    tgbot::detail::AnyMessageCallbackDispatcher dispatcher(nullptr, 1, 8);
    std::mutex mutex;
    std::condition_variable changed;
    bool blockerStarted = false;
    bool releaseBlocker = false;
    int blockerCalls = 0;
    int oldCalls = 0;
    int newCalls = 0;

    ASSERT_TRUE(dispatcher.registerCallback(
        {}, [&](TgBotApi::CPtr, const Message::Ptr&) {
            std::unique_lock lock(mutex);
            ++blockerCalls;
            if (blockerCalls == 1) {
                blockerStarted = true;
                changed.notify_all();
                changed.wait(lock, [&] { return releaseBlocker; });
            }
            changed.notify_all();
            return TgBotApi::AnyMessageResult::Handled;
        }));
    ASSERT_TRUE(dispatcher.registerCallback(
        "reloadable", [&](TgBotApi::CPtr, const Message::Ptr&) {
            const std::lock_guard lock(mutex);
            ++oldCalls;
            changed.notify_all();
            return TgBotApi::AnyMessageResult::Handled;
        }));

    ASSERT_TRUE(dispatcher.enqueue(std::make_shared<Message>()));
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(changed.wait_for(lock, 2s, [&] { return blockerStarted; }));
    }

    dispatcher.removeCallbacksForCommand("reloadable");
    ASSERT_TRUE(dispatcher.registerCallback(
        "reloadable", [&](TgBotApi::CPtr, const Message::Ptr&) {
            const std::lock_guard lock(mutex);
            ++newCalls;
            changed.notify_all();
            return TgBotApi::AnyMessageResult::Handled;
        }));

    {
        const std::lock_guard lock(mutex);
        releaseBlocker = true;
    }
    changed.notify_all();
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(
            changed.wait_for(lock, 2s, [&] { return blockerCalls == 1; }));
        EXPECT_EQ(oldCalls, 0);
        EXPECT_EQ(newCalls, 0);
    }

    ASSERT_TRUE(dispatcher.enqueue(std::make_shared<Message>()));
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(changed.wait_for(
            lock, 2s, [&] { return blockerCalls == 2 && newCalls == 1; }));
        EXPECT_EQ(oldCalls, 0);
    }
}
