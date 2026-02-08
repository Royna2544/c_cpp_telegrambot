//! Git Client test tools
//! This binary provides command-line tools to interact with Git repositories,
//! primarily for testing and debugging purposes. (For crate git_repo. See git_repo.rs)
//!
//! Features include:
//! - Getting remote url                                  (git_cli remote get-url <remote name>)
//! - Getting current branch name                         (git_cli branch)
//! - Fetching remote branches                            (git_cli fetch <branch>)
//! - Checking out branches                               (git_cli checkout <branch>)
//! - Fast-forwarding branches                            (git_cli ff <branch>)
//! - Comparing HEAD with remote branch if it is same.    (git_cli compare-remote <branch>)
//! - Cloning repositories                                (git_cli clone <repo url> <path>)
//! - Updating submodules                                 (git_cli submodule-update)
//!
//! Arguments included is --gh-api-token <TOKEN> to provide GitHub API token for
//! private repository access if needed.
//!
//! # Usage
//! ```bash
//! git-cli <COMMAND> [ARGS]
//! ```

use std::{path::PathBuf, process};

use builder::git_repo::GitRepo;
use tracing::info;
use tracing_subscriber::FmtSubscriber;

use clap::{Arg, Command, Subcommand};

use anyhow::Result;

struct Cli {
    command: Commands,
    gh_api_token: Option<String>,
}

#[derive(Subcommand)]
enum Commands {
    RemoteGetUrl {
        remote_name: String,
    },
    Branch,
    Fetch {
        branch: String,
    },
    Checkout {
        branch: String,
    },
    FastForward,
    CompareRemote {
        branch: String,
    },
    Clone {
        repo_url: String,
        branch: String,
        path: String,
    },
    SubmoduleUpdate,
}

fn run() -> Result<()> {
    let subscriber = FmtSubscriber::builder()
        .with_max_level(tracing::Level::DEBUG)
        .with_file(true)
        .with_line_number(true)
        .with_target(false)
        .finish();

    tracing::subscriber::set_global_default(subscriber).expect("setting default subscriber failed");

    let matches = Command::new("git-cli")
        .version("1.0")
        .author("Soo Hwan Na <roynatech@gmail.com>")
        .about("Git Client command-line tool for testing and debugging")
        .arg(
            Arg::new("gh-api-token")
                .long("gh-api-token")
                .required(false)
                .help("GitHub API token for private repository access"),
        )
        .arg(
            Arg::new("remote-name")
                .long("remote-name")
                .default_value("origin")
                .required(false)
                .help("Name of the remote repository"),
        )
        .subcommand_required(true)
        .subcommand(
            Command::new("remote-get-url")
                .about("Get the URL of a remote")
                .arg(
                    Arg::new("remote_name")
                        .required(true)
                        .help("Name of the remote"),
                ),
        )
        .subcommand(Command::new("branch").about("Get the current branch name"))
        .subcommand(
            Command::new("fetch")
                .about("Fetch a branch from the remote")
                .arg(Arg::new("branch").required(true).help("Branch to fetch")),
        )
        .subcommand(
            Command::new("checkout")
                .about("Checkout a branch")
                .arg(Arg::new("branch").required(true).help("Branch to checkout")),
        )
        .subcommand(Command::new("ff").about("Fast-forward current branch"))
        .subcommand(
            Command::new("compare-remote")
                .about("Compare HEAD with remote branch")
                .arg(
                    Arg::new("branch")
                        .required(true)
                        .help("Remote branch to compare with"),
                ),
        )
        .subcommand(
            Command::new("clone")
                .about("Clone a repository")
                .arg(Arg::new("repo_url").required(true).help("Repository URL"))
                .arg(Arg::new("branch").required(true).help("Branch to clone"))
                .arg(Arg::new("path").required(true).help("Path to clone into")),
        )
        .subcommand(Command::new("submodule-update").about("Update submodules"))
        .get_matches();

    let gh_api_token = matches
        .get_one::<String>("gh-api-token")
        .map(|s| s.to_string());
    let remote_name = matches
        .get_one::<String>("remote-name")
        .unwrap()
        .to_string();

    let command = match matches.subcommand() {
        Some(("remote-get-url", sub_m)) => Commands::RemoteGetUrl {
            remote_name: sub_m.get_one::<String>("remote_name").unwrap().to_string(),
        },
        Some(("branch", _)) => Commands::Branch,
        Some(("fetch", sub_m)) => Commands::Fetch {
            branch: sub_m.get_one::<String>("branch").unwrap().to_string(),
        },
        Some(("checkout", sub_m)) => Commands::Checkout {
            branch: sub_m.get_one::<String>("branch").unwrap().to_string(),
        },
        Some(("ff", _)) => Commands::FastForward,
        Some(("compare-remote", sub_m)) => Commands::CompareRemote {
            branch: sub_m.get_one::<String>("branch").unwrap().to_string(),
        },
        Some(("clone", sub_m)) => Commands::Clone {
            repo_url: sub_m.get_one::<String>("repo_url").unwrap().to_string(),
            path: sub_m.get_one::<String>("path").unwrap().to_string(),
            branch: sub_m.get_one::<String>("branch").unwrap().to_string(),
        },
        Some(("submodule-update", _)) => Commands::SubmoduleUpdate,
        _ => unreachable!("Exhausted list of subcommands and subcommand_required prevents `None`"),
    };
    let cli = Cli {
        command,
        gh_api_token,
    };

    match cli.command {
        Commands::RemoteGetUrl { remote_name } => {
            let repo = GitRepo::new(&PathBuf::from("."), &remote_name, cli.gh_api_token, None)?;
            let url = repo.get_remote_url()?;
            info!("Remote URL: {}", url);
            Ok(())
        }
        Commands::Branch => {
            let repo = GitRepo::new(&PathBuf::from("."), &remote_name, cli.gh_api_token, None)?;
            let branch_name = repo.get_branch_name()?;
            info!("Current branch: {}", branch_name);
            Ok(())
        }
        Commands::Fetch { branch } => {
            let repo = GitRepo::new(&PathBuf::from("."), &remote_name, cli.gh_api_token, None)?;
            repo.fetch_branch(&branch)?;
            info!("Fetched branch: {}", branch);
            Ok(())
        }
        Commands::Checkout { branch } => {
            let repo = GitRepo::new(&PathBuf::from("."), &remote_name, cli.gh_api_token, None)?;
            repo.checkout_branch(&branch)?;
            info!("Checked out branch: {}", branch);
            Ok(())
        }
        Commands::FastForward => {
            let repo = GitRepo::new(&PathBuf::from("."), &remote_name, cli.gh_api_token, None)?;
            repo.fast_forward()?;
            info!("Fast-forwarded branch");
            Ok(())
        }
        Commands::CompareRemote { branch } => {
            let repo = GitRepo::new(&PathBuf::from("."), &remote_name, cli.gh_api_token, None)?;
            let is_same = repo.cmp_head_with_remote_branch(&branch)?;
            if is_same {
                info!("HEAD is the same as remote branch: {}", branch);
            } else {
                info!("HEAD differs from remote branch: {}", branch);
            }
            Ok(())
        }
        Commands::Clone {
            repo_url,
            branch,
            path,
        } => {
            GitRepo::clone(
                &repo_url,
                &branch,
                None,
                &PathBuf::from(&path),
                cli.gh_api_token,
                &None,
            )?;
            info!("Cloned repository from {} to {}", repo_url, path);
            Ok(())
        }
        Commands::SubmoduleUpdate => {
            let repo = GitRepo::new(&PathBuf::from("."), &remote_name, cli.gh_api_token, None)?;
            repo.update_modules()?;
            info!("Updated submodules");
            Ok(())
        }
    }
}

fn main() {
    if let Err(e) = run() {
        // If it fails, print the error nicely to Stderr
        tracing::error!("Error: {:#}", e);

        // Exit with code 1 (failure) so scripts know it failed
        process::exit(1);
    }
}
