#include "RepoSyncTask.hpp"

#include <filesystem>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>

#include "tasks/PerBuildData.hpp"

bool RepoSyncLocalHook::process(const std::string& line) {
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
    if (std::regex_match(line, kSyncErrorNetworkRegex)) {
        LOG(INFO) << "Detected sync issue, caused by network";
        hadProblems = true;
        return true;
    }
    return false;
}

bool RepoSyncTask::runFunction() {
    try {
        RepoUtils utils(data.scriptDirectory);
        utils.repo_init({
            .url = data.rConfig.url,
            .branch = data.rConfig.branch,
        });
        if (!std::filesystem::exists(kLocalManifestPath)) {
            utils.git_clone(data.bConfig.local_manifest,
                            kLocalManifestPath.data());
        } else {
            LOG(INFO) << "Local manifest exists, skipping";
        }
        try {
            utils.repo_sync(std::thread::hardware_concurrency());
        } catch (const std::runtime_error& e) {
            // Try with lower job count
            utils.repo_sync(std::thread::hardware_concurrency() / 4);
        }
    } catch (const std::exception& e) {
        LOG(ERROR) << "Error occurred during repo sync: " << e.what();
        return false;
    }
    return true;
}

void RepoSyncTask::onNewStderrBuffer(ForkAndRun::BufferType& buffer) {
    std::vector<std::string> lines;

    boost::split(lines, buffer.data(), [](const char c) { return c == '\n'; });
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
        std::cerr << "Repo sync stderr: " << line << std::endl;
    }
}

void RepoSyncTask::onExit(int exitCode) {
    LOG(INFO) << "Repo sync exited with code: " << exitCode;
    if (localHook.hasProblems()) {
        data.result->setMessage(localHook.getLogMessage());
        data.result->value = localHook.hasFatalProblems()
                                 ? PerBuildData::Result::ERROR_FATAL
                                 : PerBuildData::Result::ERROR_NONFATAL;
    } else if (networkHook.hasProblems()) {
        data.result->setMessage(networkHook.getLogMessage());
        data.result->value = PerBuildData::Result::ERROR_NONFATAL;
    } else {
        data.result->value = PerBuildData::Result::SUCCESS;
        data.result->setMessage("Repo sync successful");
    }
    localHook.clearProblems();
    networkHook.clearProblems();
}

void RepoSyncTask::onSignal(int signalCode) {
    LOG(INFO) << "Repo sync received signal: " << strsignal(signalCode);
}

RepoSyncTask::RepoSyncTask(PerBuildData data) : data(std::move(data)) {}
