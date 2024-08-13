#include "RepoUtils.hpp"

#include <absl/log/log.h>
#include <git2.h>

#include "ArgumentBuilder.hpp"

RepoUtils::RepoUtils() {
    auto py = PythonClass::get();

    auto repomod = py->importModule("repo_utils");
    if (!repomod) {
        errorAndThrow("Cannot import repo_utils module");
    }
    repoinit_function = repomod->lookupFunction("repo_init");
    reposync_function = repomod->lookupFunction("repo_sync");

    if (!repoinit_function || !reposync_function) {
        errorAndThrow("Cannot find functions");
    }
    git_libgit2_init();
    LOG(INFO) << "RepoUtils initialized";
}

RepoUtils::~RepoUtils() {
    git_libgit2_shutdown();
    LOG(INFO) << "RepoUtils shut down";
}

void RepoUtils::repo_init(const RepoInfo& options) {
    ArgumentBuilder builder(2);
    PyObject* args = nullptr;
    bool ret = false;

    builder.add_argument(options.url);
    builder.add_argument(options.branch);
    args = builder.build();
    if (args == nullptr) {
        errorAndThrow("Failed to prepare arguments");
    }
    ret = repoinit_function->call<bool>(args, &ret);
    Py_DECREF(args);
    if (!ret) {
        errorAndThrow("Failed to initialize repository");
    } else {
        LOG(INFO) << "Repository initialized successfully";
    }
}

void RepoUtils::repo_sync(const long job_count) {
    bool ret = false;
    ArgumentBuilder builder(1);
    auto* const arg = builder.add_argument(job_count).build();
    if (arg == nullptr) {
        errorAndThrow("Failed to prepare arguments");
    }
    ret = reposync_function->call<bool>(arg, &ret);
    Py_DECREF(arg);
    if (!ret) {
        errorAndThrow("Failed to sync repository");
    } else {
        LOG(INFO) << "Repository synced successfully";
    }
}

void RepoUtils::errorAndThrow(const std::string& message) {
    LOG(ERROR) << message;
    throw std::runtime_error(message);
}

void RepoUtils::git_clone(const RepoInfo& options,
                          const std::filesystem::path& directory) {
    git_repository* repo = nullptr;
    git_clone_options gitoptions = GIT_CLONE_OPTIONS_INIT;

    LOG(INFO) << "Cloning repository, url: " << options.url << ", branch: " << options.branch;
    gitoptions.checkout_branch = options.branch.c_str();
    int ret =
        ::git_clone(&repo, options.url.c_str(), directory.c_str(), &gitoptions);
    if (ret != 0) {
        const auto* fault = git_error_last();
        errorAndThrow(fault->message);
    }
    git_repository_free(repo);
    LOG(INFO) << "Git repository cloned successfully";
}
