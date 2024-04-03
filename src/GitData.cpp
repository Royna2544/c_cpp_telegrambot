#include <GitData.h>
#include <absl/log/log.h>
#include <git2.h>

bool GitData::Fill(GitData *data) {
    std::filesystem::path path = std::filesystem::current_path();
    git_repository *repo = NULL;
    git_commit *head_commit = NULL;
    git_reference *head_ref = NULL;
    git_remote *origin = NULL;
    char head_sha[41] = {0};
    bool rc = true;

    // Initial invalid ret
    int error = -1;

    git_libgit2_init();

    // Open the repository, try going up the directory tree until we find a .git
    // folder
    error = git_repository_open(&repo, path.string().c_str());
    if (error != 0) {
        for (; path.has_parent_path(); path = path.parent_path()) {
            if (path.root_path() == path) {
                LOG(ERROR) << "Error opening git repository";
                return false;
            }
            error = git_repository_open(&repo, path.string().c_str());
            if (error == 0) {
                break;
            }
        }
        if (error != 0) {
            LOG(ERROR) << "Not a git repository (or any of the parent directories)";
            return false;
        }
    }
    data->gitSrcRoot = path;

    error = git_repository_head(&head_ref, repo);
    if (error == 0) {
        const git_oid *head_oid = git_reference_target(head_ref);
        error = git_commit_lookup(&head_commit, repo, head_oid);
        git_reference_free(head_ref);
    } else {
        LOG(ERROR) << "Error getting HEAD commit: " << giterr_last()->message;
        git_repository_free(repo);
        return false;
    }

    // Get the SHA1 (OID) of the HEAD commit
    git_oid_fmt(head_sha, git_commit_id(head_commit));

    // Get the origin URL
    error = git_remote_lookup(&origin, repo, "origin");
    if (error == 0) {
        data->originurl = git_remote_url(origin);
        data->commitmsg = git_commit_message(head_commit);
        data->commitid = head_sha;
        git_remote_free(origin);
    } else {
        LOG(ERROR) << "Error getting origin URL: " << giterr_last()->message;
        rc = false;
    }

    // Clean up
    git_commit_free(head_commit);
    git_repository_free(repo);
    git_libgit2_shutdown();
    return rc;
}

bool GitData::Fill() { return Fill(this); }