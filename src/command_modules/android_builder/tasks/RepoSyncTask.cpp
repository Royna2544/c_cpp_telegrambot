#include "RepoSyncTask.hpp"

#include <absl/strings/str_split.h>
#include <git2.h>
#include <git2/clone.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <internal/raii.hpp>
#include <ios>
#include <regex>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

#include "CompileTimeStringConcat.hpp"
#include "ForkAndRun.hpp"
#include "RepoUtils.hpp"
#include "libos/libfs.hpp"

bool RepoSyncLocalHook::process(const std::string& line) {
    static const std::regex kRepoRemoveFail(
        "error: ([a-zA-Z0-9_\\-\\/]+): Cannot remove project: "
        "uncommitted changes are present\\.");
    std::smatch smatch;

    if (line.find(kUpdatingFiles) != std::string::npos) {
        // Stop logspam
        return true;
    }
    if (line.find(kChangesWouldBeLost) != std::string::npos) {
        errorAndLog(
            "Repo sync failed due to local issue: Changes would be lost");
        hadProblems = true;
        return true;
    }

    if (line.find(kRepoCheckoutFailStart) != std::string::npos) {
        errorAndLog("Repo sync failed due to local issue: CheckoutFail");
        hasCheckoutIssues = true;
        hadProblems = true;
        return true;
    }

    if (std::regex_search(line, smatch, kRepoRemoveFail)) {
        std::error_code ec;
        errorAndLog("Repo sync failed due to local issue: RemoveFail");
        hasRemoveIssues = true;
        hadProblems = true;
        if (smatch.size() > 1) {
            std::string directory = smatch[1].str();
            LOG(WARNING) << "Will remove directory: " << directory;
            std::filesystem::remove_all(directory, ec);
            if (ec) {
                errorAndLog("Failed to remove: " + directory + ": " +
                            ec.message());
                hadFatalProblems = true;
            } else {
                LOG(INFO) << "Successfully removed";
            }
        }
    }

    if (hasCheckoutIssues) {
        std::error_code ec;
        if (line.find(kRepoCheckoutFailEnd) != std::string::npos) {
            // Clear the flag
            hasCheckoutIssues = false;
            return true;
        }
        std::filesystem::remove_all(line, ec);
        std::string msg = "Checkout Failed Directory: " + line + ": ";
        if (ec) {
            errorAndLog(msg + "Failed to remove: " + ec.message());
            hadFatalProblems = true;
        } else {
            errorAndLog(msg + "Removed");
        }
        return true;
    }
    return false;
}

bool RepoSyncNetworkHook::process(const std::string& line) {
    static const std::regex kSyncErrorNetworkRegex(
        R"(^error:\s+Cannot\s+fetch\s+[^\s]+(?:/[^\s]+)*\s+from\s+https:\/\/.+$)");
    static const std::regex kSyncErrorNetworkRegex2(
        R"(^Failed to connect to (github\.com|gitlab\.com) port \d+ after \d+ ms: Couldn't connect to server$)");
    if (std::regex_match(line, kSyncErrorNetworkRegex) ||
        std::regex_match(line, kSyncErrorNetworkRegex2)) {
        LOG(INFO) << "Detected sync issue, caused by network: " << line;
        hadProblems = true;
        return true;
    }
    return false;
}

DeferredExit RepoSyncTask::runFunction() {
    std::error_code ec;
    bool repoDirExists = false;

    repoDirExists = std::filesystem::is_directory(".repo/manifests.git", ec);
    if (ec &&
        ec != std::make_error_code(std::errc::no_such_file_or_directory)) {
        LOG(ERROR) << "Failed to check .repo directory existence: "
                   << ec.message();
        return DeferredExit::generic_fail;
    } else {
        DLOG(INFO) << ".repo directory exists: " << std::boolalpha
                   << repoDirExists;
    }

    const auto& rom = getValue(data.localManifest->rom);
    GitBranchSwitcher switcher{.gitDirectory = ".repo/manifests.git",
                               .desiredBranch = rom->branch,
                               .desiredUrl = rom->romInfo->url,
                               .checkout = false};
    if (!repoDirExists || !switcher()) {
        auto r = RepoUtils::repo_init({rom->romInfo->url, rom->branch});
        if (!r) {
            return r;
        }
    }
    if (const auto val = walk_up_tree_and_gather<false>(
            [](const std::filesystem::path& path) {
                DLOG(INFO) << "Walking up: " << path;
                return std::filesystem::exists(path / ".repo");
            });
        val.size() != 0) {
        for (const auto& dir : val) {
            LOG(ERROR) << "Found .repo directory in parent directory: "
                       << dir.string();
            std::filesystem::remove_all(dir / ".repo");
        }
    }
    if (!(*data.localManifest->prepare)(kLocalManifestPath.data())) {
        LOG(ERROR) << "Failed to prepare local manifest";
        return DeferredExit::generic_fail;
    }
    unsigned int job_count = std::thread::hardware_concurrency() / 2;
    DeferredExit rs;
    if (runWithReducedJobs) {
        // Futher reduce the number of jobs count
        rs = RepoUtils::repo_sync(job_count / 4);
    } else {
        rs = RepoUtils::repo_sync(job_count);
    }
    if (!rs) {
        return rs;
    }
    return {};  // Success
}

void RepoSyncTask::onNewStderrBuffer(ForkAndRun::BufferType& buffer) {
    std::vector<std::string> lines;

    // Split the buffer into lines
    lines = absl::StrSplit(buffer.data(), '\n');
    if (std::chrono::system_clock::now() - clock > 10s) {
        clock = std::chrono::system_clock::now();
        wrapper->editMessage(message, "Sync in progress...:\n" + lines[0]);
    }
    for (const auto& line : lines) {
        if (line.empty()) {
            continue;
        }
        if (localHook.process(line)) {
            continue;
        }
        if (networkHook.process(line)) {
            continue;
        }
        std::cout << line << std::endl;
    }
}

void RepoSyncTask::onExit(int exitCode) {
    LOG(INFO) << "Repo sync exited with code: " << exitCode;
    auto* result = data.result;
    if (localHook.hasProblems()) {
        result->setMessage(localHook.getLogMessage());
        result->value = localHook.hasFatalProblems()
                            ? PerBuildData::Result::ERROR_FATAL
                            : PerBuildData::Result::ERROR_NONFATAL;
    } else if (networkHook.hasProblems()) {
        result->setMessage(networkHook.getLogMessage());
        result->value = PerBuildData::Result::ERROR_NONFATAL;
        if (runWithReducedJobs) {
            // Second try failed, lets get out of here.
            result->value = PerBuildData::Result::ERROR_FATAL;
            result->setMessage("Repo sync failed, even with retries");
            return;
        }
        // Will be picked up by next loop
        runWithReducedJobs = true;
    } else if (exitCode != 0) {
        result->value = PerBuildData::Result::ERROR_FATAL;
        result->setMessage("Repo sync failed");
    } else {
        result->value = PerBuildData::Result::SUCCESS;
        result->setMessage("Repo sync successful");
    }
    localHook.clearProblems();  // Per-sync only, retrying won't help
}

void RepoSyncTask::onSignal(int signalCode) {
    LOG(INFO) << "Repo sync received signal: " << strsignal(signalCode);
}

RepoSyncTask::RepoSyncTask(ApiPtr wrapper, Message::Ptr message,
                           PerBuildData data)
    : data(std::move(data)), wrapper(wrapper), message(std::move(message)) {}
