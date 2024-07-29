#include <../src/command_modules/support/popen_wdt/popen_wdt.h>
#include <gtest/gtest.h>

#include <StringResManager.hpp>

#include "CommandModulesTest.hpp"

struct BashCommandTest : public CommandTestBase {
    BashCommandTest() : CommandTestBase("bash") {}
};

struct UBashCommandTest : public CommandTestBase {
    UBashCommandTest() : CommandTestBase("ubash") {}
};

TEST_F(BashCommandTest, PwdCommand) {
    setCommandExtArgs("pwd");

    // First, "Working on it...\nCommand is: pwd\n"
    // Edit, to add the exec done.
    willSendReplyMessage(GETSTR(WORKING) + "pwd\n")
        .willEdit(StartsWith(GETSTR(WORKING)));

    // Second, Command result of pwd command
    willSendMessage(StartsWith(current_path()));

    // Execute
    execute();
}

TEST_F(BashCommandTest, NoCommand) {
    setCommandExtArgs();
    willSendReplyMessage(StartsWith("Error"));
    execute();
}

TEST_F(BashCommandTest, watchdogTimeout) {
    // Test the watchdog timeout
    LOG(INFO) << "Testing watchdog timeout";

    const std::string command = "sleep 20";
    // Set command to sleep 20 seconds
    setCommandExtArgs(command);
    // "Working on it...\nCommand is: sleep 20"
    willSendReplyMessage(GETSTR(WORKING) + command + "\n")
        // Sends total time
        .willEdit(EndsWith("milliseconds\n"))
        // Sends watchdog timeout
        .willEdit(EndsWith("WDT TIMEOUT\n"))
        // After 15 seconds, sends nothing
        .willEdit(EndsWith("Output is empty\n"));

    DurationPoint dp;
    execute();
    const auto tookTime = dp.get();

    // Shouldn't take more than a second to SLEEP_SECONDS
    if (tookTime > std::chrono::seconds(SLEEP_SECONDS) + 2s) {
        FAIL() << "Watchdog wasn't triggered: Took " << tookTime.count()
               << " milliseconds";
    } else {
        LOG(INFO) << "Took " << tookTime.count() << " milliseconds, pass";
    }
}

TEST_F(UBashCommandTest, PwdCommand) {
    setCommandExtArgs("pwd");

    // First, "Working on it...\nCommand is: pwd\n"
    // Edit, to add the exec done.
    willSendReplyMessage(GETSTR(WORKING) + "pwd\n")
        .willEdit(StartsWith(GETSTR(WORKING)));

    // Second, Command result of pwd command
    willSendMessage(StartsWith(current_path()));

    // Execute
    execute();
}

TEST_F(UBashCommandTest, NoCommand) {
    // Test without any command following
    setCommandExtArgs();
    willSendReplyMessage(StartsWith("Error"));
    execute();
}

TEST_F(UBashCommandTest, WatchdogTimeout) {
    // Test the watchdog timeout
    LOG(INFO) << "Testing watchdog timeout";

    const std::string command = "sleep 20";

    // Set command to sleep 20 seconds
    setCommandExtArgs(command);

    // "Working on it...\nCommand is: sleep 20"
    willSendReplyMessage(GETSTR(WORKING) + "sleep 20\n")
        // Sends total time
        .willEdit(EndsWith("milliseconds\n"))
        // After 15 seconds, sends nothing
        .willEdit(EndsWith("Output is empty\n"));

    DurationPoint dp;
    execute();
    const auto tookTime = dp.get();

    // Shouldn't get timeout kill, compare to more than a second to
    // SLEEP_SECONDS
    if (tookTime < std::chrono::seconds(SLEEP_SECONDS) + 2s) {
        FAIL() << "Watchdog was triggered: Took " << tookTime.count()
               << " milliseconds";
    } else {
        LOG(INFO) << "Took " << tookTime.count() << " milliseconds, pass";
    }
}
