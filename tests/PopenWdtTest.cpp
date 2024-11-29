#include <fmt/chrono.h>
#include <fmt/core.h>
#include <gtest/gtest.h>
#include <popen_wdt.h>

#include <chrono>

#include "DurationPoint.hpp"

TEST(PopenWdtTest, Init) {
    popen_watchdog_data_t* data = nullptr;
    EXPECT_TRUE(popen_watchdog_init(&data));
    popen_watchdog_destroy(&data);
}

TEST(PopenWdtTest, NonBlockingCommand) {
    popen_watchdog_data_t* data = nullptr;
    EXPECT_TRUE(popen_watchdog_init(&data));
    data->watchdog_enabled = false;
    data->command = "ls";
    ASSERT_TRUE(popen_watchdog_start(&data));
    EXPECT_FALSE(popen_watchdog_activated(&data));
    auto ret = popen_watchdog_destroy(&data);
    EXPECT_EQ(ret.exitcode, 0);
    EXPECT_FALSE(ret.signal);
}

TEST(PopenWdtTest, BlockingCommand) {
    popen_watchdog_data_t* data = nullptr;
    EXPECT_TRUE(popen_watchdog_init(&data));
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
    EXPECT_TRUE(popen_watchdog_init(&data));
    data->watchdog_enabled = true;
    data->sleep_secs = 4;
    data->command = "ls";
    ASSERT_TRUE(popen_watchdog_start(&data));
    EXPECT_FALSE(popen_watchdog_activated(&data));
    auto ret = popen_watchdog_destroy(&data);
    EXPECT_EQ(ret.exitcode, 0);
    EXPECT_FALSE(ret.signal);
}

TEST(PopenWdtTest, BlockingCommandEnabled) {
    popen_watchdog_data_t* data = nullptr;
    EXPECT_TRUE(popen_watchdog_init(&data));
    data->watchdog_enabled = true;
    data->sleep_secs = 1;
    data->command = "sleep 9";
    SecondDP dp;
    ASSERT_TRUE(popen_watchdog_start(&data));
    EXPECT_TRUE(popen_watchdog_activated(&data));
    auto ret = popen_watchdog_destroy(&data);
    EXPECT_EQ(ret.exitcode, POPEN_WDT_SIGTERM);
    EXPECT_TRUE(ret.signal);
    auto tp = dp.get();
    fmt::print("Took {}\n", tp);
    EXPECT_LE(tp, std::chrono::seconds(3));
}


TEST(PopenWdtTest, BlockingCommandEnabledMultiFork) {
    popen_watchdog_data_t* data = nullptr;
    EXPECT_TRUE(popen_watchdog_init(&data));
    data->watchdog_enabled = true;
    data->sleep_secs = 1;
    data->command = R"(bash -c "sleep 9; sh -c "CUM"")";
    SecondDP dp;
    ASSERT_TRUE(popen_watchdog_start(&data));
    EXPECT_TRUE(popen_watchdog_activated(&data));
    auto ret = popen_watchdog_destroy(&data);
    EXPECT_EQ(ret.exitcode, POPEN_WDT_SIGTERM);
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