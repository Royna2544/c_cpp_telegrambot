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
//! repo.fast_forward().await.expect("Failed to fast-forward");
//! # };
//! ```

use std::{num::NonZero, path::PathBuf};

use crate::util::LogErr;

use super::ratelimit::RateLimit;
use git2::Repository;
use tracing::{debug, error, info, warn};

pub struct GitRepo {
    repo: Repository,
    remote_name: String,
    cred_callback: Box<CredCallback>,
    progress_callback: Option<Box<ProgressCallback>>,
    ratelimit: RateLimit,
}

type CredCallback =
    dyn Fn(&str, Option<&str>, git2::CredentialType) -> Result<git2::Cred, git2::Error>;
pub type ProgressCallback = dyn Fn(&git2::Progress) + Send + Sync;

impl GitRepo {
    fn get_cred_callback(github_token: Option<String>) -> Box<CredCallback> {
        Box::new(move |url, username_from_url, _allowed_types| {
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
            if url.contains("github.com") {
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
        path: &PathBuf,
        remote_name: &str,
        github_token: Option<String>,
        progress_callback: Option<Box<ProgressCallback>>,
    ) -> Result<Self, git2::Error> {
        let repo = Repository::open(path).log_if_err("Cannot open repository")?;

        Ok(GitRepo {
            repo,
            remote_name: remote_name.to_string(),
            progress_callback,
            cred_callback: Self::get_cred_callback(github_token),
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
        ro.credentials(self.cred_callback.as_ref());
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
        dest_path: &PathBuf,
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

        match builder.clone(url, &dest_path) {
            Ok(_) => info!("Successfully cloned {} into {}", url, dest_path.display()),
            Err(e) => {
                error!("Failed to clone repository: {}", e);
                return Err(e);
            }
        };

        Self::update_submodules(&Repository::open(&dest_path)?)?;
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
