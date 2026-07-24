//! Git repository management and operations.
//!
//! This module provides a wrapper around libgit2 for common Git operations
//! used in the builder service, including:
//!
//! - Repository cloning with authentication
//! - Branch checkout and fast-forward merging
//! - Submodule updates
//! - Progress tracking with rate limiting
//!
//! # Examples
//!
//! ```no_run
//! # use std::path::PathBuf;
//! # use builder::git_repo::GitRepo;
//!
//! # async {
//! let repo = GitRepo::new(
//!     &PathBuf::from("/path/to/repo"),
//!     "origin",
//!     None, // No GitHub token
//!     None, // No progress callback
//! ).expect("Failed to open repository");
//!
//! repo.fast_forward().expect("Failed to fast-forward");
//! # };
//! ```

use std::{cell::RefCell, num::NonZero, path::Path};

use crate::util::LogErr;

use super::ratelimit::RateLimit;
use git2::Repository;
use tracing::{debug, error, info, warn};

pub struct GitRepo {
    repo: Repository,
    remote_name: String,
    github_token: Option<String>,
    progress_callback: Option<Box<ProgressCallback>>,
    ratelimit: RateLimit,
}

type CredCallback =
    dyn Fn(&str, Option<&str>, git2::CredentialType) -> Result<git2::Cred, git2::Error>;
pub type ProgressCallback = dyn Fn(&git2::Progress) + Send + Sync;

impl GitRepo {
    fn is_token_eligible_url(url: &str) -> bool {
        reqwest::Url::parse(url).is_ok_and(|parsed| {
            parsed.scheme() == "https" && parsed.host_str() == Some("github.com")
        })
    }

    /// Builds a libgit2 credential callback. Attempt tracking lives inside the
    /// closure, so a fresh callback must be created for every clone/fetch.
    ///
    /// libgit2 re-invokes the callback after the server rejects the offered
    /// credentials. There is no interactive fallback here, so a retry would
    /// replay the same credentials until libgit2 hits its replay cap and
    /// reports an opaque "too many redirects or authentication replays"
    /// error — instead, a second request for the same URL is answered with a
    /// clear authentication error.
    fn get_cred_callback(github_token: Option<String>) -> Box<CredCallback> {
        let last_url: RefCell<Option<String>> = RefCell::new(None);
        Box::new(move |url, username_from_url, allowed_types| {
            // A USERNAME-only query is libgit2 asking which username to use
            // (ssh URLs without one embedded), not an authentication attempt.
            if allowed_types == git2::CredentialType::USERNAME {
                return git2::Cred::username(username_from_url.unwrap_or("git"));
            }
            if last_url.borrow().as_deref() == Some(url) {
                error!("Credentials for {} were rejected by the server", url);
                return Err(git2::Error::new(
                    git2::ErrorCode::Auth,
                    git2::ErrorClass::Callback,
                    format!(
                        "authentication failed for '{url}': credentials were rejected by the remote server"
                    ),
                ));
            }
            *last_url.borrow_mut() = Some(url.to_string());
            // Try to open default config, fall back to new empty config if that fails
            let config = git2::Config::open_default()
                .inspect_err(|x| {
                    warn!("Opening new, empty gitconfig due to: {}", x);
                })
                .or_else(|_| git2::Config::new())
                .expect("Failed to initialize git config");
            let username = username_from_url.unwrap_or("git");
            if url.starts_with("ssh://") {
                debug!("SSH URL detected for authentication.");
                return git2::Cred::ssh_key_from_agent(username);
            }
            if Self::is_token_eligible_url(url) {
                debug!("GitHub URL detected for authentication.");
                if let Some(token) = &github_token {
                    info!("Using provided GitHub token for authentication.");
                    let token_str = token.as_str();
                    return git2::Cred::userpass_plaintext(token_str, "");
                }
            }
            debug!("Using credential helper for authentication.");
            match git2::Cred::credential_helper(&config, url, Some(username)) {
                Ok(cred) => return Ok(cred),
                Err(e) => {
                    error!("Credential helper failed: {}", e);
                }
            }
            git2::Cred::default()
        })
    }

    fn with_rate_limit<F>(&self, mut callback: F) -> impl FnMut(git2::Progress<'_>) -> bool
    where
        // F is any closure user passes in
        F: FnMut(&git2::Progress<'_>),
    {
        // logic: accepts value 'p', passes ref '&p' to inner callback
        move |p| {
            if self.ratelimit.check() {
                callback(&p);
                info!(
                    "[git stats] rx:{}/t:{} objects (idx:{})",
                    p.received_objects(),
                    p.total_objects(),
                    p.indexed_objects()
                );
            }
            true
        }
    }

    pub fn new(
        path: &Path,
        remote_name: &str,
        github_token: Option<String>,
        progress_callback: Option<Box<ProgressCallback>>,
    ) -> Result<Self, git2::Error> {
        let repo = Repository::open(path).log_if_err("Cannot open repository")?;

        Ok(GitRepo {
            repo,
            remote_name: remote_name.to_string(),
            progress_callback,
            github_token,
            ratelimit: RateLimit::new(NonZero::new(5).unwrap()),
        })
    }

    pub fn get_remote_url(&self) -> Result<String, git2::Error> {
        let remote = self
            .repo
            .find_remote(&self.remote_name)
            .log_if_err("Cannot find remote")?;
        match remote.url() {
            None => {
                return Err(git2::Error::from_str(&format!(
                    "Remote {} has no URL",
                    self.remote_name
                )));
            }
            Some(url) => debug!("Found remote URL: {} for remote {}", url, self.remote_name),
        }
        Ok(remote.url().unwrap_or("").to_string())
    }

    pub fn get_branch_name(&self) -> Result<String, git2::Error> {
        let head = self.repo.head().log_if_err("Cannot get HEAD")?;
        match head.shorthand() {
            Some(name) => {
                debug!("Current branch name: {}", name);
                Ok(name.to_string())
            }
            None => Err(git2::Error::from_str("Failed to get branch name")),
        }
    }

    pub fn fetch_branch(&self, branch: &str) -> Result<(), git2::Error> {
        let mut fo = git2::FetchOptions::new();
        let mut ro = git2::RemoteCallbacks::new();
        // Fresh callback per operation so its auth-attempt tracking resets.
        ro.credentials(Self::get_cred_callback(self.github_token.clone()));
        if let Some(progress_cb) = &self.progress_callback {
            ro.transfer_progress(self.with_rate_limit(progress_cb));
        }
        fo.remote_callbacks(ro);
        let mut remote = self
            .repo
            .find_remote(&self.remote_name)
            .log_if_err("Cannot find remote")?;
        let refspec = format!(
            "refs/heads/{}:refs/remotes/{}/{}",
            branch, self.remote_name, branch
        );
        remote
            .fetch(&[&refspec], Some(&mut fo), None)
            .log_if_err("Cannot fetch remote")?;
        // Create FETCH_HEAD reference
        info!("Fetched branch {} into FETCH_HEAD", branch);
        Ok(())
    }

    pub fn checkout_branch(&self, branch: &str) -> Result<(), git2::Error> {
        // 1. Try to find local branch first
        let local_refname = format!("refs/heads/{}", branch);
        let target_obj = match self.repo.revparse_single(&local_refname) {
            Ok(obj) => obj,
            Err(_) => {
                // 2. Not found locally, let's fetch
                info!(
                    "Branch {} not found locally, fetching from remote...",
                    branch
                );
                self.fetch_branch(branch)?;

                // 3. Look for the remote tracking branch (e.g., refs/remotes/origin/main)
                let remote_refname = format!("refs/remotes/{}/{}", self.remote_name, branch);
                let remote_obj = self
                    .repo
                    .revparse_single(&remote_refname)
                    .log_if_err("Branch not found on remote after fetch")?;

                // 4. Create the local branch pointing to the remote commit
                let commit = remote_obj.peel_to_commit()?;
                self.repo.branch(branch, &commit, false)?;
                remote_obj
            }
        };

        // 5. Perform the actual checkout (updates files in workdir)
        self.repo.checkout_tree(&target_obj, None)?;

        // 6. Point HEAD to the local branch (so it's not detached)
        self.repo.set_head(&local_refname)?;

        info!("Successfully checked out branch: {}", branch);
        Ok(())
    }

    pub fn fast_forward(&self) -> Result<(), git2::Error> {
        let config_branch = self.get_branch_name()?;
        self.fetch_branch(&config_branch)?;
        let fetch_head = self.repo.find_reference("FETCH_HEAD")?;
        let fetch_commit = self.repo.reference_to_annotated_commit(&fetch_head)?;
        let analysis = self.repo.merge_analysis(&[&fetch_commit])?;
        if analysis.0.is_fast_forward() {
            let target_commit = self.repo.find_commit(fetch_commit.id())?;

            // Mimic 'git pull --ff-only' behavior
            let mut checkout_opts = git2::build::CheckoutBuilder::new();
            checkout_opts.force(); // Overwrite local modifications
            checkout_opts.remove_untracked(true); // Delete conflicting folders/submodules
            checkout_opts.recreate_missing(true); // Restore deleted files

            // 1. Checkout the tree of the new commit FIRST
            // This updates Index and Workdir to match the new commit
            self.repo
                .checkout_tree(target_commit.as_object(), Some(&mut checkout_opts))?;

            // 2. Update the reference
            let refname = format!("refs/heads/{}", config_branch);
            let mut rhead = self.repo.find_reference(&refname)?;
            rhead.set_target(fetch_commit.id(), "Fast-Forward")?;

            let local_ref = self.repo.find_reference(&refname)?;
            // 5. Update HEAD to point to the new commit (if we are currently ON this branch)
            if self.repo.head()?.name() == local_ref.name() {
                self.repo.set_head(local_ref.name().unwrap())?;
                self.repo.checkout_head(Some(&mut checkout_opts))?;
            }

            info!("Fast-forwarded branch: {}", config_branch);
        }

        Self::update_submodules(&self.repo)?;
        Ok(())
    }

    pub fn cmp_head_with_remote_branch(&self, branch: &str) -> Result<bool, git2::Error> {
        let head = self
            .repo
            .head()
            .warn_err("Cannot resolve HEAD")?
            .peel_to_commit()
            .warn_err("Cannot resolve HEAD to a commit")?;
        let branch = format!("{}/{}", self.remote_name, branch);
        let branch_ref = self
            .repo
            .find_branch(&branch, git2::BranchType::Remote)
            .warn_err_string(format!("Cannot find remote branch {} by name", &branch))?;
        let branch_commit = branch_ref.get().peel_to_commit()?;

        info!(
            "Comparing HEAD (id: {}) with remote branch {} (id: {})",
            head.id(),
            branch,
            branch_commit.id()
        );
        Ok(head.id() == branch_commit.id())
    }

    pub fn clone(
        url: &str,
        branch: &str,
        clone_depth: Option<i32>,
        dest_path: &Path,
        github_token: Option<String>,
        progress_callback: &Option<Box<ProgressCallback>>,
    ) -> Result<(), git2::Error> {
        let mut cb = git2::RemoteCallbacks::new();

        info!("Cloning repository from {} to {}", url, dest_path.display());
        let cred = Self::get_cred_callback(github_token);
        cb.credentials(cred);
        if let Some(progress_cb) = &progress_callback {
            cb.transfer_progress(move |progress| {
                progress_cb(&progress);
                true
            });
        }

        let mut fo = git2::FetchOptions::new();
        fo.remote_callbacks(cb);
        if let Some(depth) = clone_depth {
            info!("Setting clone depth to {}", depth);
            fo.depth(depth);
        }

        let mut builder = git2::build::RepoBuilder::new();
        builder.fetch_options(fo);
        builder.branch(branch);
        info!("Starting clone operation...");

        match builder.clone(url, dest_path) {
            Ok(_) => info!("Successfully cloned {} into {}", url, dest_path.display()),
            Err(e) => {
                error!("Failed to clone repository: {}", e);
                return Err(e);
            }
        };

        Self::update_submodules(&Repository::open(dest_path)?)?;
        Ok(())
    }

    fn update_submodules(repo: &Repository) -> Result<(), git2::Error> {
        for mut submodule in repo.submodules()? {
            info!(
                "Updating submodule: {}",
                submodule.name().unwrap_or("unnamed")
            );
            submodule.update(true, None)?;
        }
        info!("Successfully updated submodules.");
        Ok(())
    }

    pub fn update_modules(&self) -> Result<(), git2::Error> {
        Self::update_submodules(&self.repo)
    }
}

/// A handle to an opened git repository — the instance operations the build
/// services rely on. Implemented by [`GitRepo`] in production and by a mock in
/// tests, so the build workflow can be exercised without touching real repos.
///
/// Not `Send`: handles are used within a single task and not held across
/// awaits, exactly as the concrete `GitRepo` was before this seam.
pub trait RepoHandle {
    fn get_remote_url(&self) -> Result<String, git2::Error>;
    fn get_branch_name(&self) -> Result<String, git2::Error>;
    fn fetch_branch(&self, branch: &str) -> Result<(), git2::Error>;
    fn checkout_branch(&self, branch: &str) -> Result<(), git2::Error>;
    fn fast_forward(&self) -> Result<(), git2::Error>;
    fn cmp_head_with_remote_branch(&self, branch: &str) -> Result<bool, git2::Error>;
    fn update_modules(&self) -> Result<(), git2::Error>;
}

impl RepoHandle for GitRepo {
    fn get_remote_url(&self) -> Result<String, git2::Error> {
        GitRepo::get_remote_url(self)
    }
    fn get_branch_name(&self) -> Result<String, git2::Error> {
        GitRepo::get_branch_name(self)
    }
    fn fetch_branch(&self, branch: &str) -> Result<(), git2::Error> {
        GitRepo::fetch_branch(self, branch)
    }
    fn checkout_branch(&self, branch: &str) -> Result<(), git2::Error> {
        GitRepo::checkout_branch(self, branch)
    }
    fn fast_forward(&self) -> Result<(), git2::Error> {
        GitRepo::fast_forward(self)
    }
    fn cmp_head_with_remote_branch(&self, branch: &str) -> Result<bool, git2::Error> {
        GitRepo::cmp_head_with_remote_branch(self, branch)
    }
    fn update_modules(&self) -> Result<(), git2::Error> {
        GitRepo::update_modules(self)
    }
}

/// Seam for git operations: opens/clones repositories and hands back
/// [`RepoHandle`]s. Production uses [`RealGitProvider`]; tests inject a mock.
pub trait GitProvider: Send + Sync {
    fn open(
        &self,
        path: &Path,
        remote_name: &str,
        github_token: Option<String>,
        progress_callback: Option<Box<ProgressCallback>>,
    ) -> Result<Box<dyn RepoHandle>, git2::Error>;

    // Named `clone_repo`, not `clone`, so `Arc<dyn GitProvider>.clone_repo(..)`
    // doesn't collide with `Arc`'s `Clone::clone`.
    fn clone_repo(
        &self,
        url: &str,
        branch: &str,
        clone_depth: Option<i32>,
        dest_path: &Path,
        github_token: Option<String>,
        progress_callback: &Option<Box<ProgressCallback>>,
    ) -> Result<(), git2::Error>;
}

/// Real git provider backed by libgit2 (the production implementation).
pub struct RealGitProvider;

impl GitProvider for RealGitProvider {
    fn open(
        &self,
        path: &Path,
        remote_name: &str,
        github_token: Option<String>,
        progress_callback: Option<Box<ProgressCallback>>,
    ) -> Result<Box<dyn RepoHandle>, git2::Error> {
        Ok(Box::new(GitRepo::new(
            path,
            remote_name,
            github_token,
            progress_callback,
        )?))
    }

    fn clone_repo(
        &self,
        url: &str,
        branch: &str,
        clone_depth: Option<i32>,
        dest_path: &Path,
        github_token: Option<String>,
        progress_callback: &Option<Box<ProgressCallback>>,
    ) -> Result<(), git2::Error> {
        GitRepo::clone(
            url,
            branch,
            clone_depth,
            dest_path,
            github_token,
            progress_callback,
        )
    }
}

#[cfg(test)]
mod cred_callback_tests {
    //! Exercises the credential callback directly. URLs are chosen so no
    //! external process is invoked: github.com + token short-circuits before
    //! the credential helper, and ssh:// never reaches it.
    use super::GitRepo;

    #[test]
    fn github_token_is_only_used_for_exact_https_github_host() {
        assert!(GitRepo::is_token_eligible_url("https://github.com/foo/bar"));
        assert!(GitRepo::is_token_eligible_url(
            "https://github.com:443/foo/bar"
        ));
        assert!(!GitRepo::is_token_eligible_url("http://github.com/foo/bar"));
        assert!(!GitRepo::is_token_eligible_url("ssh://github.com/foo/bar"));
        assert!(!GitRepo::is_token_eligible_url(
            "https://github.com.evil.test/foo/bar"
        ));
        assert!(!GitRepo::is_token_eligible_url(
            "https://evil.test/github.com/foo/bar"
        ));
        assert!(!GitRepo::is_token_eligible_url(
            "not a URL containing github.com"
        ));
    }

    #[test]
    fn second_attempt_for_same_url_is_an_auth_error() {
        let cb = GitRepo::get_cred_callback(Some("test-token".to_string()));
        let url = "https://github.com/foo/bar";
        cb(url, None, git2::CredentialType::USER_PASS_PLAINTEXT)
            .expect("first attempt should yield token credentials");
        // libgit2 asking again for the same URL means the server rejected them.
        let err = match cb(url, None, git2::CredentialType::USER_PASS_PLAINTEXT) {
            Ok(_) => panic!("second attempt for the same URL should fail"),
            Err(e) => e,
        };
        assert_eq!(err.code(), git2::ErrorCode::Auth);
        assert!(
            err.message().contains(url),
            "error should name the URL: {err}"
        );
    }

    #[test]
    fn a_different_url_gets_a_fresh_attempt() {
        let cb = GitRepo::get_cred_callback(Some("test-token".to_string()));
        cb(
            "https://github.com/foo/a",
            None,
            git2::CredentialType::USER_PASS_PLAINTEXT,
        )
        .expect("first URL should yield credentials");
        // A new URL (e.g. after a redirect) is not a rejection of the previous one.
        cb(
            "https://github.com/foo/b",
            None,
            git2::CredentialType::USER_PASS_PLAINTEXT,
        )
        .expect("a different URL should get a fresh attempt");
        let err = match cb(
            "https://github.com/foo/b",
            None,
            git2::CredentialType::USER_PASS_PLAINTEXT,
        ) {
            Ok(_) => panic!("second attempt for the same URL should fail"),
            Err(e) => e,
        };
        assert_eq!(err.code(), git2::ErrorCode::Auth);
    }

    #[test]
    fn username_query_does_not_count_as_an_attempt() {
        let cb = GitRepo::get_cred_callback(None);
        let url = "ssh://example.com/foo/bar";
        cb(url, None, git2::CredentialType::USERNAME).expect("username query should be answered");
        // The follow-up SSH_KEY request is the first real attempt. It may fail
        // when no ssh-agent is reachable, but it must not be misreported as
        // the server rejecting credentials.
        if let Err(e) = cb(url, Some("git"), git2::CredentialType::SSH_KEY) {
            assert_ne!(e.code(), git2::ErrorCode::Auth, "unexpected rejection: {e}");
        }
    }
}

#[cfg(test)]
#[allow(dead_code)]
pub(crate) mod mock {
    //! In-memory git seam for tests: records open/clone calls and returns canned
    //! handles, performing no real git I/O.
    use super::{GitProvider, ProgressCallback, RepoHandle};
    use std::path::Path;
    use std::sync::Mutex;

    #[derive(Default)]
    pub(crate) struct MockRepoHandle {
        pub remote_url: String,
        pub branch_name: String,
    }

    impl RepoHandle for MockRepoHandle {
        fn get_remote_url(&self) -> Result<String, git2::Error> {
            Ok(self.remote_url.clone())
        }
        fn get_branch_name(&self) -> Result<String, git2::Error> {
            Ok(self.branch_name.clone())
        }
        fn fetch_branch(&self, _branch: &str) -> Result<(), git2::Error> {
            Ok(())
        }
        fn checkout_branch(&self, _branch: &str) -> Result<(), git2::Error> {
            Ok(())
        }
        fn fast_forward(&self) -> Result<(), git2::Error> {
            Ok(())
        }
        fn cmp_head_with_remote_branch(&self, _branch: &str) -> Result<bool, git2::Error> {
            Ok(true)
        }
        fn update_modules(&self) -> Result<(), git2::Error> {
            Ok(())
        }
    }

    #[derive(Default)]
    pub(crate) struct MockGitProvider {
        pub opens: Mutex<Vec<String>>,
        pub clones: Mutex<Vec<String>>,
        pub remote_url: String,
        pub branch_name: String,
        /// When set, open/clone_repo return a git error after recording the
        /// call — to exercise the "git op failed → handle → end" paths.
        pub fail_open: bool,
        pub fail_clone: bool,
    }

    impl GitProvider for MockGitProvider {
        fn open(
            &self,
            path: &Path,
            _remote_name: &str,
            _github_token: Option<String>,
            _progress_callback: Option<Box<ProgressCallback>>,
        ) -> Result<Box<dyn RepoHandle>, git2::Error> {
            self.opens.lock().unwrap().push(path.display().to_string());
            if self.fail_open {
                return Err(git2::Error::from_str("mock git open failure"));
            }
            Ok(Box::new(MockRepoHandle {
                remote_url: self.remote_url.clone(),
                branch_name: self.branch_name.clone(),
            }))
        }

        fn clone_repo(
            &self,
            url: &str,
            _branch: &str,
            _clone_depth: Option<i32>,
            _dest_path: &Path,
            _github_token: Option<String>,
            _progress_callback: &Option<Box<ProgressCallback>>,
        ) -> Result<(), git2::Error> {
            self.clones.lock().unwrap().push(url.to_string());
            if self.fail_clone {
                return Err(git2::Error::from_str("mock git clone failure"));
            }
            Ok(())
        }
    }
}
