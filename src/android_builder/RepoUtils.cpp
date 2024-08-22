#include "RepoUtils.hpp"

#include <absl/log/log.h>
#include <git2.h>

#include <source_location>
#include <stdexcept>

#include "ArgumentBuilder.hpp"

std::runtime_error createError(
    const char* __restrict message,
    const std::source_location& st = std::source_location::current()) {
    LOG(ERROR) << "[Line " << st.line() << "] " << message;
    return std::runtime_error(message);
}

RepoUtils::RepoUtils() {
    auto py = PythonClass::get();

    auto repomod = py->importModule("repo_utils");
    if (!repomod) {
        throw createError("Cannot import repo_utils module");
    }
    repoinit_function = repomod->lookupFunction("repo_init");
    reposync_function = repomod->lookupFunction("repo_sync");

    if (!repoinit_function || !reposync_function) {
        throw createError("Cannot find required functions");
    }
    git_libgit2_init();
    DLOG(INFO) << "RepoUtils initialized";
}

RepoUtils::~RepoUtils() {
    git_libgit2_shutdown();
    DLOG(INFO) << "RepoUtils shut down";
}

void RepoUtils::repo_init(const RepoInfo& options) {
    ArgumentBuilder builder(2);
    PyObject* args = nullptr;
    bool call_ret = false;
    bool function_ret = false;

    builder.add_argument(options.url);
    builder.add_argument(options.branch);
    args = builder.build();
    if (args == nullptr) {
        throw createError("Failed to prepare arguments");
    }
    call_ret = repoinit_function->call<bool>(args, &function_ret);
    Py_DECREF(args);
    if (!call_ret || !function_ret) {
        throw createError("Failed to initialize repository");
    } else {
        LOG(INFO) << "Repository initialized successfully";
    }
}

void RepoUtils::repo_sync(const long job_count) {
    bool call_ret = false;
    bool function_ret = false;

    ArgumentBuilder builder(1);
    auto* const arg = builder.add_argument(job_count).build();
    if (arg == nullptr) {
        throw createError("Failed to prepare arguments");
    }
    call_ret = reposync_function->call<bool>(arg, &function_ret);
    Py_DECREF(arg);
    if (!call_ret || !function_ret) {
        throw createError("Failed to sync repository");
    } else {
        LOG(INFO) << "Repository synced successfully";
    }
}

void RepoUtils::git_clone(const RepoInfo& options,
                          const std::filesystem::path& directory) {
    git_repository* repo = nullptr;
    git_clone_options gitoptions = GIT_CLONE_OPTIONS_INIT;

    LOG(INFO) << "Cloning repository, url: " << options.url
              << ", branch: " << options.branch;
    gitoptions.checkout_branch = options.branch.c_str();
    int ret =
        ::git_clone(&repo, options.url.c_str(), directory.c_str(), &gitoptions);
    if (ret != 0) {
        const auto* fault = git_error_last();
        throw createError(fault->message);
    }
    git_repository_free(repo);
    LOG(INFO) << "Git repository cloned successfully";
}
