#include <gtest/gtest.h>

#include <api/components/Async.hpp>
#include <api/components/ModuleExecutionContext.hpp>
#include <api/components/WorkScheduler.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

TEST(WorkScheduler, StalledMediaDoesNotBlockOutboundWork) {
    TgBotApiImpl::WorkScheduler scheduler;
    std::mutex mutex;
    std::condition_variable mediaStarted;
    bool started = false;

    auto media = scheduler.submit("media-owner", TgBotApi::WorkClass::Media,
                                  [&](std::stop_token stop) {
                                      {
                                          const std::lock_guard lock(mutex);
                                          started = true;
                                      }
                                      mediaStarted.notify_one();
                                      while (!stop.stop_requested())
                                          std::this_thread::sleep_for(5ms);
                                  },
                                  {.deadline = 5s});
    ASSERT_TRUE(media.has_value());
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(mediaStarted.wait_for(lock, 1s, [&] { return started; }));
    }

    std::promise<void> outboundDone;
    auto outbound =
        scheduler.submit("outbound-owner", TgBotApi::WorkClass::Outbound,
                         [&](std::stop_token) { outboundDone.set_value(); });
    ASSERT_TRUE(outbound.has_value());
    EXPECT_EQ(outboundDone.get_future().wait_for(1s),
              std::future_status::ready);
    EXPECT_TRUE(scheduler.cancel("media-owner", *media));
    scheduler.cancelAndDrain("media-owner");
}

TEST(WorkScheduler, StalledAskConfirmationsDoNotOccupyFastCommandWorkers) {
    TgBotApiImpl::WorkScheduler scheduler;
    TgBotApiImpl::Async fastCommands("test-fast-commands", 2, 8);

    std::mutex startedMutex;
    std::condition_variable startedCondition;
    std::size_t startedCount = 0;
    std::promise<std::optional<TgBotApi::WorkId>> firstSubmitted;
    std::promise<std::optional<TgBotApi::WorkId>> secondSubmitted;

    const auto enqueueAsk = [&](auto& submitted) {
        auto* submittedResult = &submitted;
        return fastCommands.emplaceTask(
            "ask", [&scheduler, &startedMutex, &startedCondition, &startedCount,
                    submittedResult] {
                // This is all the fast command callback does: enqueue the work
                // and return. The LLM lane may then spend the full confirmation
                // window waiting for a human without retaining either fast
                // worker.
                submittedResult->set_value(scheduler.submit(
                    "ask", TgBotApi::WorkClass::Llm,
                    [&startedMutex, &startedCondition,
                     &startedCount](std::stop_token stop) {
                        {
                            const std::lock_guard lock(startedMutex);
                            ++startedCount;
                        }
                        startedCondition.notify_all();
                        const auto confirmationDeadline =
                            std::chrono::steady_clock::now() + 90s;
                        while (!stop.stop_requested() &&
                               std::chrono::steady_clock::now() <
                                   confirmationDeadline) {
                            std::this_thread::sleep_for(2ms);
                        }
                    }));
            });
    };

    ASSERT_TRUE(enqueueAsk(firstSubmitted));
    ASSERT_TRUE(enqueueAsk(secondSubmitted));
    auto firstFuture = firstSubmitted.get_future();
    auto secondFuture = secondSubmitted.get_future();
    ASSERT_EQ(firstFuture.wait_for(1s), std::future_status::ready);
    ASSERT_EQ(secondFuture.wait_for(1s), std::future_status::ready);
    const auto firstId = firstFuture.get();
    const auto secondId = secondFuture.get();
    ASSERT_TRUE(firstId.has_value());
    ASSERT_TRUE(secondId.has_value());
    {
        std::unique_lock lock(startedMutex);
        ASSERT_TRUE(startedCondition.wait_for(
            lock, 1s, [&] { return startedCount == 2; }));
    }

    std::promise<void> fastAlive;
    auto fastAliveFuture = fastAlive.get_future();
    ASSERT_TRUE(fastCommands.emplaceTask(
        "alive", [&fastAlive] { fastAlive.set_value(); }));
    EXPECT_EQ(fastAliveFuture.wait_for(1s), std::future_status::ready);

    std::promise<void> outboundAlive;
    auto outboundAliveFuture = outboundAlive.get_future();
    ASSERT_TRUE(scheduler
                    .submit("ask", TgBotApi::WorkClass::Outbound,
                            [&outboundAlive](std::stop_token) {
                                outboundAlive.set_value();
                            })
                    .has_value());
    EXPECT_EQ(outboundAliveFuture.wait_for(1s), std::future_status::ready);

    EXPECT_TRUE(scheduler.cancel("ask", *firstId));
    EXPECT_TRUE(scheduler.cancel("ask", *secondId));
    scheduler.cancelAndDrain("ask");
}

TEST(WorkScheduler, DeadlineRequestsCooperativeCancellation) {
    TgBotApiImpl::WorkScheduler scheduler;
    std::promise<void> cancelled;
    auto work = scheduler.submit("deadline", TgBotApi::WorkClass::Process,
                                 [&](std::stop_token stop) {
                                     while (!stop.stop_requested())
                                         std::this_thread::sleep_for(2ms);
                                     cancelled.set_value();
                                 },
                                 {.deadline = 30ms});
    ASSERT_TRUE(work.has_value());
    EXPECT_EQ(cancelled.get_future().wait_for(1s), std::future_status::ready);
    scheduler.cancelAndDrain("deadline");
}

TEST(WorkScheduler, UnboundedProcessAllowsOnlyOneOutstandingJob) {
    TgBotApiImpl::WorkScheduler scheduler;
    std::promise<void> started;
    auto first =
        scheduler.submit("ubash", TgBotApi::WorkClass::UnboundedProcess,
                         [&](std::stop_token stop) {
                             started.set_value();
                             while (!stop.stop_requested())
                                 std::this_thread::sleep_for(2ms);
                         });
    ASSERT_TRUE(first.has_value());
    ASSERT_EQ(started.get_future().wait_for(1s), std::future_status::ready);

    EXPECT_FALSE(scheduler
                     .submit("ubash", TgBotApi::WorkClass::UnboundedProcess,
                             [](std::stop_token) {})
                     .has_value());
    EXPECT_TRUE(scheduler.cancel("ubash", *first));
    scheduler.cancelAndDrain("ubash");
}

TEST(WorkScheduler, DelayedWorkCanBeCancelledBeforeExecution) {
    TgBotApiImpl::WorkScheduler scheduler;
    std::atomic_bool ran = false;
    auto work = scheduler.submit("delay", TgBotApi::WorkClass::Outbound,
                                 [&](std::stop_token) { ran = true; },
                                 {.delay = 500ms});
    ASSERT_TRUE(work.has_value());
    EXPECT_TRUE(scheduler.cancel("delay", *work));
    scheduler.cancelAndDrain("delay");
    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(ran.load());
}

TEST(WorkScheduler, WorkRunsInOwningModuleExecutionScope) {
    TgBotApiImpl::WorkScheduler scheduler;
    std::promise<bool> observed;
    auto future = observed.get_future();

    ASSERT_TRUE(
        scheduler
            .submit("scheduled-owner", TgBotApi::WorkClass::Outbound,
                    [&observed](std::stop_token) {
                        observed.set_value(
                            module_execution::isExecuting("scheduled-owner"));
                    })
            .has_value());
    ASSERT_EQ(future.wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(future.get());
    scheduler.cancelAndDrain("scheduled-owner");
}
