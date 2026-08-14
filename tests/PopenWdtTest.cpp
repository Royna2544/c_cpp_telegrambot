#include <fmt/chrono.h>
#include <fmt/core.h>
#include <gtest/gtest.h>
#include <popen_wdt.h>

#include <barrier>
#include <chrono>
#include <cstdlib>
#include <future>
#include <string>
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#else
#include <cerrno>
#include <csignal>
#endif

#include "DurationPoint.hpp"

TEST(PopenWdtTest, Init) {
    popen_watchdog_data_t* data = nullptr;
    ASSERT_TRUE(popen_watchdog_init(&data));
    popen_watchdog_destroy(&data);
}

TEST(PopenWdtTest, NonBlockingCommand) {
    popen_watchdog_data_t* data = nullptr;
    DLOG(INFO) << "popen_watchdog_init";
    ASSERT_TRUE(popen_watchdog_init(&data));
    data->watchdog_enabled = false;
    data->command = "pwd";
    DLOG(INFO) << "popen_watchdog_start";
    ASSERT_TRUE(popen_watchdog_start(&data));
    DLOG(INFO) << "popen_watchdog_activated";
    EXPECT_FALSE(popen_watchdog_activated(&data));
    DLOG(INFO) << "popen_watchdog_destroy";
    auto ret = popen_watchdog_destroy(&data);
    EXPECT_EQ(ret.exitcode, 0);
    EXPECT_FALSE(ret.signal);
}

TEST(PopenWdtTest, TestingEchoOutput) {
    popen_watchdog_data_t* data = nullptr;
    ASSERT_TRUE(popen_watchdog_init(&data));
    data->watchdog_enabled = false;
    data->command = "echo test";
    ASSERT_TRUE(popen_watchdog_start(&data));
    char buf[5] = {0};
    EXPECT_TRUE(popen_watchdog_read(&data, buf, sizeof(buf) - 1));
    EXPECT_STREQ(buf, "test");
    EXPECT_FALSE(popen_watchdog_activated(&data));
    auto ret = popen_watchdog_destroy(&data);
    EXPECT_EQ(ret.exitcode, 0);
    EXPECT_FALSE(ret.signal);
}

TEST(PopenWdtTest, BlockingCommand) {
    popen_watchdog_data_t* data = nullptr;
    ASSERT_TRUE(popen_watchdog_init(&data));
    data->watchdog_enabled = false;
    data->sleep_secs = 1;
    data->command = "sleep 5";
    ASSERT_TRUE(popen_watchdog_start(&data));
    EXPECT_FALSE(popen_watchdog_activated(&data));
    auto ret = popen_watchdog_destroy(&data);
    EXPECT_EQ(ret.exitcode, 0);
    EXPECT_FALSE(ret.signal);
}

TEST(PopenWdtTest, NonBlockingCommandEnabled) {
    popen_watchdog_data_t* data = nullptr;
    ASSERT_TRUE(popen_watchdog_init(&data));
    data->watchdog_enabled = true;
    data->sleep_secs = 4;
    data->command = "pwd";
    ASSERT_TRUE(popen_watchdog_start(&data));
    EXPECT_FALSE(popen_watchdog_activated(&data));
    auto ret = popen_watchdog_destroy(&data);
    EXPECT_EQ(ret.exitcode, 0);
    EXPECT_FALSE(ret.signal);
}

TEST(PopenWdtTest, BlockingCommandEnabled) {
    popen_watchdog_data_t* data = nullptr;
    ASSERT_TRUE(popen_watchdog_init(&data));
    data->watchdog_enabled = true;
    data->sleep_secs = 1;
    data->command = "sleep 9";
    SecondDP dp;
    ASSERT_TRUE(popen_watchdog_start(&data));
    EXPECT_TRUE(popen_watchdog_activated(&data));
    auto ret = popen_watchdog_destroy(&data);
#ifndef _WIN32
    EXPECT_EQ(ret.exitcode, POPEN_WDT_SIGTERM);
#else
    EXPECT_NE(ret.exitcode, POPEN_WDT_EXIT_CODE_MAX);
#endif
    EXPECT_TRUE(ret.signal);
    auto tp = dp.get();
    fmt::print("Took {}\n", tp);
    EXPECT_LE(tp, std::chrono::seconds(3));
}

TEST(PopenWdtTest, BlockingCommandEnabledMultiFork) {
    popen_watchdog_data_t* data = nullptr;
    ASSERT_TRUE(popen_watchdog_init(&data));
    data->watchdog_enabled = true;
    data->sleep_secs = 1;
    data->command = POPEN_WDT_DEFAULT_SHELL
        " -c \"sleep 9\"; " POPEN_WDT_DEFAULT_SHELL " -c lll";
    SecondDP dp;
    ASSERT_TRUE(popen_watchdog_start(&data));
    EXPECT_TRUE(popen_watchdog_activated(&data));
    auto ret = popen_watchdog_destroy(&data);
#ifndef _WIN32
    EXPECT_EQ(ret.exitcode, POPEN_WDT_SIGTERM);
#else
    EXPECT_NE(ret.exitcode, POPEN_WDT_EXIT_CODE_MAX);
#endif
    EXPECT_TRUE(ret.signal);
    auto tp = dp.get();
    fmt::print("Took {}\n", tp);
    EXPECT_LE(tp, std::chrono::seconds(3));
}

TEST(PopenWdtTest, NothingDestroy) {
    popen_watchdog_data_t* data = nullptr;
    auto ret = popen_watchdog_destroy(&data);
    popen_watchdog_exit_t empty = POPEN_WDT_EXIT_INITIALIZER;
    EXPECT_EQ(ret.exitcode, empty.exitcode);
    EXPECT_EQ(ret.signal, empty.signal);
}

#ifdef _WIN32
TEST(PopenWdtTest, ConcurrentStartsKeepPipeInstancesIsolated) {
    popen_watchdog_data_t* first = nullptr;
    popen_watchdog_data_t* second = nullptr;
    ASSERT_TRUE(popen_watchdog_init(&first));
    ASSERT_TRUE(popen_watchdog_init(&second));
    first->watchdog_enabled = false;
    second->watchdog_enabled = false;
    first->command =
        "powershell.exe -NoLogo -NoProfile -NonInteractive -Command "
        "\"Write-Output alpha; Start-Sleep -Milliseconds 200\"";
    second->command =
        "powershell.exe -NoLogo -NoProfile -NonInteractive -Command "
        "\"Write-Output beta; Start-Sleep -Milliseconds 200\"";

    std::barrier start_gate(3);
    auto first_start = std::async(std::launch::async, [&] {
        start_gate.arrive_and_wait();
        return popen_watchdog_start(&first);
    });
    auto second_start = std::async(std::launch::async, [&] {
        start_gate.arrive_and_wait();
        return popen_watchdog_start(&second);
    });
    start_gate.arrive_and_wait();
    ASSERT_TRUE(first_start.get());
    ASSERT_TRUE(second_start.get());

    const auto read_all = [](popen_watchdog_data_t** data) {
        std::string output;
        char chunk[32] = {0};
        for (;;) {
            const auto bytes = popen_watchdog_read(data, chunk, sizeof(chunk));
            if (bytes <= 0) {
                return output;
            }
            output.append(chunk, static_cast<std::size_t>(bytes));
        }
    };
    const auto first_output = read_all(&first);
    const auto second_output = read_all(&second);
    EXPECT_EQ(first_output, "alpha\r\n");
    EXPECT_EQ(second_output, "beta\r\n");

    EXPECT_FALSE(popen_watchdog_activated(&first));
    EXPECT_FALSE(popen_watchdog_activated(&second));
    EXPECT_EQ(popen_watchdog_destroy(&first).exitcode, 0U);
    EXPECT_EQ(popen_watchdog_destroy(&second).exitcode, 0U);
}

TEST(PopenWdtTest, CancelTerminatesDescendantInJobObject) {
    popen_watchdog_data_t* data = nullptr;
    ASSERT_TRUE(popen_watchdog_init(&data));
    data->watchdog_enabled = false;
    data->command =
        "powershell.exe -NoLogo -NoProfile -NonInteractive -Command \"$p = "
        "Start-Process powershell.exe -WindowStyle Hidden -ArgumentList "
        "'-NoLogo','-NoProfile','-NonInteractive','-Command','Start-Sleep "
        "-Seconds 60' -PassThru; Write-Output $p.Id; Wait-Process -Id "
        "$p.Id\"";
    ASSERT_TRUE(popen_watchdog_start(&data));

    char output[64] = {0};
    const auto bytes = popen_watchdog_read(&data, output, sizeof(output) - 1);
    ASSERT_GT(bytes, 0);
    output[bytes] = '\0';
    char* end = nullptr;
    const DWORD child_pid = (DWORD)strtoul(output, &end, 10);
    ASSERT_NE(end, output);
    ASSERT_NE(child_pid, 0U);

    HANDLE child = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
                               FALSE, child_pid);
    ASSERT_NE(child, nullptr);
    EXPECT_TRUE(popen_watchdog_cancel(&data));
    EXPECT_EQ(WaitForSingleObject(child, 1000), WAIT_OBJECT_0);
    CloseHandle(child);

    const auto result = popen_watchdog_destroy(&data);
    EXPECT_TRUE(result.signal);
}

TEST(PopenWdtTest, CancelCanPreemptAConcurrentStatusWaiter) {
    popen_watchdog_data_t* data = nullptr;
    ASSERT_TRUE(popen_watchdog_init(&data));
    data->watchdog_enabled = false;
    data->command =
        "powershell.exe -NoLogo -NoProfile -NonInteractive -Command "
        "\"Start-Sleep -Seconds 30\"";
    ASSERT_TRUE(popen_watchdog_start(&data));

    auto waiter = std::async(std::launch::async,
                             [&] { return popen_watchdog_activated(&data); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(popen_watchdog_cancel(&data));
    ASSERT_EQ(waiter.wait_for(std::chrono::seconds(7)),
              std::future_status::ready);
    EXPECT_TRUE(waiter.get());

    EXPECT_TRUE(popen_watchdog_destroy(&data).signal);
}
#endif

#ifndef _WIN32
TEST(PopenWdtTest, ConcurrentStartsDoNotCrossRetainOutputPipes) {
    popen_watchdog_data_t* first = nullptr;
    popen_watchdog_data_t* second = nullptr;
    ASSERT_TRUE(popen_watchdog_init(&first));
    ASSERT_TRUE(popen_watchdog_init(&second));
    first->watchdog_enabled = false;
    second->watchdog_enabled = false;
    first->command = "printf alpha; sleep 3";
    second->command = "printf beta";

    std::barrier startGate(3);
    auto firstStart = std::async(std::launch::async, [&] {
        startGate.arrive_and_wait();
        return popen_watchdog_start(&first);
    });
    auto secondStart = std::async(std::launch::async, [&] {
        startGate.arrive_and_wait();
        return popen_watchdog_start(&second);
    });
    startGate.arrive_and_wait();
    ASSERT_TRUE(firstStart.get());
    ASSERT_TRUE(secondStart.get());

    char firstOutput[16] = {0};
    char secondOutput[16] = {0};
    const auto firstBytes =
        popen_watchdog_read(&first, firstOutput, sizeof(firstOutput) - 1);
    const auto secondBytes =
        popen_watchdog_read(&second, secondOutput, sizeof(secondOutput) - 1);
    ASSERT_GT(firstBytes, 0);
    ASSERT_GT(secondBytes, 0);
    firstOutput[firstBytes] = '\0';
    secondOutput[secondBytes] = '\0';
    EXPECT_STREQ(firstOutput, "alpha");
    EXPECT_STREQ(secondOutput, "beta");

    char eofBuffer[1] = {0};
    auto secondEof = std::async(std::launch::async, [&] {
        return popen_watchdog_read(&second, eofBuffer, sizeof(eofBuffer));
    });
    EXPECT_EQ(secondEof.wait_for(std::chrono::milliseconds(500)),
              std::future_status::ready);
    EXPECT_TRUE(popen_watchdog_cancel(&first));
    EXPECT_EQ(secondEof.get(), -1);

    EXPECT_TRUE(popen_watchdog_destroy(&first).signal);
    EXPECT_EQ(popen_watchdog_destroy(&second).exitcode, 0);
}

TEST(PopenWdtTest, PreservesShellExitStatus) {
    popen_watchdog_data_t* data = nullptr;
    ASSERT_TRUE(popen_watchdog_init(&data));
    data->watchdog_enabled = false;
    data->command = "exit 37";
    ASSERT_TRUE(popen_watchdog_start(&data));
    EXPECT_FALSE(popen_watchdog_activated(&data));
    const auto result = popen_watchdog_destroy(&data);
    EXPECT_FALSE(result.signal);
    EXPECT_EQ(result.exitcode, 37);
}

TEST(PopenWdtTest, TimeoutAllowsTermHandlerToPreserveExitStatus) {
    popen_watchdog_data_t* data = nullptr;
    ASSERT_TRUE(popen_watchdog_init(&data));
    data->watchdog_enabled = true;
    data->sleep_secs = 1;
    data->command = "trap 'exit 42' TERM; while :; do sleep 1; done";
    ASSERT_TRUE(popen_watchdog_start(&data));
    EXPECT_TRUE(popen_watchdog_activated(&data));
    const auto result = popen_watchdog_destroy(&data);
    EXPECT_FALSE(result.signal);
    EXPECT_EQ(result.exitcode, 42);
}

TEST(PopenWdtTest, CancelEscalatesIgnoredTermToKill) {
    popen_watchdog_data_t* data = nullptr;
    ASSERT_TRUE(popen_watchdog_init(&data));
    data->watchdog_enabled = false;
    data->command = "trap '' TERM; while :; do sleep 1; done";
    ASSERT_TRUE(popen_watchdog_start(&data));
    EXPECT_TRUE(popen_watchdog_cancel(&data));
    const auto result = popen_watchdog_destroy(&data);
    EXPECT_TRUE(result.signal);
    EXPECT_EQ(result.exitcode, POPEN_WDT_SIGKILL);
}

TEST(PopenWdtTest, CancelCanPreemptAConcurrentStatusWaiter) {
    popen_watchdog_data_t* data = nullptr;
    ASSERT_TRUE(popen_watchdog_init(&data));
    data->watchdog_enabled = false;
    data->command = "sleep 30";
    ASSERT_TRUE(popen_watchdog_start(&data));

    auto waiter = std::async(std::launch::async,
                             [&] { return popen_watchdog_activated(&data); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(popen_watchdog_cancel(&data));
    ASSERT_EQ(waiter.wait_for(std::chrono::seconds(3)),
              std::future_status::ready);
    EXPECT_TRUE(waiter.get());

    const auto result = popen_watchdog_destroy(&data);
    EXPECT_TRUE(result.signal);
    EXPECT_EQ(result.exitcode, POPEN_WDT_SIGTERM);
}

TEST(PopenWdtTest, DestroyContainsBackgroundDescendantsAndPreservesLeaderExit) {
    popen_watchdog_data_t* data = nullptr;
    ASSERT_TRUE(popen_watchdog_init(&data));
    data->watchdog_enabled = false;
    data->command =
        "sleep 30 & child=$!; printf '%s:%s\\n' \"$$\" \"$child\"; exit 23";
    ASSERT_TRUE(popen_watchdog_start(&data));

    char output[64] = {0};
    const auto bytes = popen_watchdog_read(&data, output, sizeof(output) - 1);
    ASSERT_GT(bytes, 0);
    output[bytes] = '\0';
    char* separator = nullptr;
    const auto process_group = strtol(output, &separator, 10);
    ASSERT_NE(separator, output);
    ASSERT_EQ(*separator, ':');

    EXPECT_FALSE(popen_watchdog_activated(&data));
    const auto result = popen_watchdog_destroy(&data);
    EXPECT_FALSE(result.signal);
    EXPECT_EQ(result.exitcode, 23);

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(1);
    int group_result = 0;
    do {
        errno = 0;
        group_result = kill(-static_cast<pid_t>(process_group), 0);
        if (group_result == -1 && errno == ESRCH) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() < deadline);
    EXPECT_EQ(group_result, -1);
    EXPECT_EQ(errno, ESRCH);
}
#endif
