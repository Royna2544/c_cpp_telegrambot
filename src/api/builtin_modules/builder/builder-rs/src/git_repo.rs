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

use crate::ratelimit;

use super::ratelimit::RateLimit;
use git2::Repository;
use tracing::{debug, error, info};

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
type ProgressCallbackForGit = dyn FnMut(git2::Progress) -> bool + Send + Sync;

impl GitRepo {
    fn get_cred_callback(github_token: Option<String>) -> Box<CredCallback> {
        Box::new(move |url, username_from_url, _allowed_types| {
            // Try to open default config, fall back to new empty config if that fails
            let config = git2::Config::open_default()
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
            return git2::Cred::credential_helper(&config, url, Some(username));
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
        let repo = Repository::open(path).inspect_err(|e| {
            error!("Cannot open repo at {:?}, {}", &path, e);
        })?;

        Ok(GitRepo {
            repo,
            remote_name: remote_name.to_string(),
            progress_callback,
            cred_callback: Self::get_cred_callback(github_token),
            ratelimit: RateLimit::new(NonZero::new(5).unwrap()),
        })
    }

    pub fn get_remote_url(&self) -> Result<String, git2::Error> {
        let remote = self.repo.find_remote(&self.remote_name)?;
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
        let head = self.repo.head()?;
        match head.shorthand() {
            Some(name) => {
                debug!("Current branch name: {}", name);
                Ok(name.to_string())
            }
            None => Err(git2::Error::from_str("Failed to get branch name")),
        }
    }

    pub fn checkout_branch(&self, branch: &str) -> Result<(), git2::Error> {
        let (object, reference) = self.repo.revparse_ext(branch)?;
        self.repo.checkout_tree(&object, None)?;
        match reference {
            Some(gref) => self.repo.set_head(gref.name().unwrap()),
            None => self.repo.set_head_detached(object.id()),
        }?;
        info!("Checked out branch: {}", branch);
        Ok(())
    }

    pub fn fast_forward(&self) -> Result<(), git2::Error> {
        let mut fo = git2::FetchOptions::new();
        let mut ro = git2::RemoteCallbacks::new();
        ro.credentials(self.cred_callback.as_ref());
        if let Some(progress_cb) = &self.progress_callback {
            ro.transfer_progress(self.with_rate_limit(progress_cb));
        }
        fo.remote_callbacks(ro);

        let config_branch = self.get_branch_name()?;
        let mut remote = self.repo.find_remote(&self.remote_name)?;
        remote.fetch(&[&config_branch], Some(&mut fo), None)?;
        let fetch_head = self.repo.find_reference("FETCH_HEAD")?;
        let fetch_commit = self.repo.reference_to_annotated_commit(&fetch_head)?;
        let analysis = self.repo.merge_analysis(&[&fetch_commit])?;
        if analysis.0.is_fast_forward() {
            let target_commit = self.repo.find_commit(fetch_commit.id())?;

            // 1. Checkout the tree of the new commit FIRST
            // This updates Index and Workdir to match the new commit
            self.repo.checkout_tree(
                target_commit.as_object(),
                Some(git2::build::CheckoutBuilder::new().safe()),
            )?;

            // 2. Update the reference
            let refname = format!("refs/heads/{}", config_branch);
            let mut rhead = self.repo.find_reference(&refname)?;
            rhead.set_target(fetch_commit.id(), "Fast-Forward")?;

            info!("Fast-forwarded branch: {}", config_branch);
        }

        Self::update_submodules(&self.repo)?;
        Ok(())
    }

    pub fn cmp_head_with_branch(&self, branch: &str) -> Result<bool, git2::Error> {
        let head = self.repo.head()?.peel_to_commit()?;
        let branch_ref = self.repo.find_branch(branch, git2::BranchType::Local)?;
        let branch_commit = branch_ref.get().peel_to_commit()?;

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
                submodule.name().unwrap_or("Unnamed")
            );
            submodule.update(true, None)?;
        }
        Ok(())
    }

    pub fn update_modules(&self) -> Result<(), git2::Error> {
        Self::update_submodules(&self.repo)
    }
}
