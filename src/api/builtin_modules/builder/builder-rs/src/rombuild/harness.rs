use super::{
    config_resolver::{ConfigType, ResolvedRomConfig},
    domain::{RomBuildTask, RomUploadMethod},
    grpc::grpc_pb::{BuildLogEntry, LogLevel},
    service::{BuildService, BuildStatus, BuildTask, UploadTask},
};
use crate::build_common::{
    ArtifactKind, BuildArtifact, BuildError, BuildEvent, BuildEventSender, BuildTerminalState,
    CommandSpec, ProcessRunner,
};
use async_trait::async_trait;
#[cfg(unix)]
use nix::libc::{rlimit, setrlimit};
use std::{path::PathBuf, sync::Arc};
use tokio::{
    process::Command,
    sync::{broadcast, mpsc},
};
use tracing::{Instrument, error, info, warn};
use xml::writer::XmlEvent;

struct RomProcessEvents {
    tx: broadcast::Sender<BuildLogEntry>,
}

#[async_trait]
impl BuildEventSender for RomProcessEvents {
    async fn send(&self, event: BuildEvent) {
        let (level, message, timestamp, is_finished) = match event {
            BuildEvent::Stdout { line, at, .. } => (LogLevel::Info, line, at.timestamp(), false),
            BuildEvent::Stderr { line, at, .. } => (LogLevel::Error, line, at.timestamp(), false),
            BuildEvent::Started { build_id, at } => (
                LogLevel::Info,
                format!("Build {build_id} started"),
                at.timestamp(),
                false,
            ),
            BuildEvent::StepStarted { name, at } => (
                LogLevel::Info,
                format!("Step started: {name}"),
                at.timestamp(),
                false,
            ),
            BuildEvent::StepFinished { name, at } => (
                LogLevel::Info,
                format!("Step finished: {name}"),
                at.timestamp(),
                false,
            ),
            BuildEvent::Progress { message, at, .. } => {
                (LogLevel::Info, message, at.timestamp(), false)
            }
            BuildEvent::Warning { message, at } => {
                (LogLevel::Warning, message, at.timestamp(), false)
            }
            BuildEvent::Error { message, at } => (LogLevel::Error, message, at.timestamp(), false),
            BuildEvent::ArtifactFound { artifact, at } => (
                LogLevel::Info,
                format!("Artifact found: {}", artifact.path.display()),
                at.timestamp(),
                false,
            ),
            BuildEvent::UploadStarted { artifact, at } => (
                LogLevel::Info,
                format!("Upload started: {}", artifact.file_name),
                at.timestamp(),
                false,
            ),
            BuildEvent::UploadFinished { artifact, url, at } => (
                LogLevel::Info,
                format!("Upload finished: {} ({url})", artifact.file_name),
                at.timestamp(),
                false,
            ),
            BuildEvent::Finished { outcome, at } => {
                let level = match outcome.state {
                    BuildTerminalState::Success => LogLevel::Info,
                    BuildTerminalState::Failed => LogLevel::Error,
                    BuildTerminalState::Cancelled => LogLevel::Fatal,
                };
                (
                    level,
                    outcome.message.unwrap_or_else(|| "Build finished".into()),
                    at.timestamp(),
                    true,
                )
            }
        };
        let _ = self.tx.send(BuildLogEntry {
            timestamp,
            level: level.into(),
            message,
            is_finished,
        });
    }
}

async fn run_process(
    runner: &dyn ProcessRunner,
    command: Command,
    tx: &broadcast::Sender<BuildLogEntry>,
    kill_rx: Option<&mut mpsc::Receiver<()>>,
    log_path: Option<PathBuf>,
    stdin_rx: Option<mpsc::Receiver<String>>,
) -> Result<bool, BuildError> {
    runner
        .run(
            CommandSpec::new(command)
                .with_log_path(log_path)
                .with_stdin(stdin_rx),
            Arc::new(RomProcessEvents { tx: tx.clone() }),
            kill_rx,
        )
        .await
        .map(|outcome| outcome.success)
}

pub(crate) struct RomBuildHarness;

impl RomBuildHarness {
    /// The build pipeline, extracted from `start_build`'s spawned task so it is
    /// directly awaitable: tests inject mock runner/git/fs via `BuildTask` and
    /// assert how a failing command / git / filesystem op is handled and ends.
    pub(crate) async fn run(task: BuildTask) -> Result<(), BuildError> {
        let BuildTask {
            domain,
            log_tx_clone,
            registry,
            askpass_path_clone,
            runner,
            git,
            fs,
            mut kill_rx,
            span,
        } = task;
        let RomBuildTask {
            id,
            request: req,
            settings: build_settings,
            dirs,
            resolved,
        } = domain;
        let ResolvedRomConfig {
            config_entry,
            device_entry,
            branch_entry,
            rom_entry,
            rom_branch_entry,
        } = resolved;
        let build_id_clone = id.into_string();
        let build_dir_clone = dirs.build;
        let tempdir_clone = dirs.temp;
        let force_checkout = req.force_checkout;
        let parallel_jobs = req.parallel_jobs;
        info!(
            "Executing ROM build config '{}' for {} (ROM '{}', Android {})",
            req.config_name, req.target_codename, req.rom_name, req.rom_android_version
        );
        let artifact_kind = match &config_entry {
            ConfigType::Standard(_) => ArtifactKind::RomZip,
            ConfigType::Recovery(_) => ArtifactKind::RecoveryImage,
        };
        macro_rules! send_log {
            ($level:expr, $msg:expr) => {
                match $level {
                    LogLevel::Debug => info!("{}", $msg),
                    LogLevel::Info => info!("{}", $msg),
                    LogLevel::Warning => warn!("{}", $msg),
                    LogLevel::Error => error!("{}", $msg),
                    LogLevel::Fatal => error!("{}", $msg),
                }
                let _ = log_tx_clone
                    .send(BuildLogEntry {
                        level: $level.into(),
                        message: $msg,
                        timestamp: chrono::Utc::now().timestamp(),
                        is_finished: false,
                    })
                    .inspect_err(|e| {
                        error!("Failed to send log entry: {}", e);
                    });
            };
        }
        macro_rules! send_log_final {
            ($level:expr, $msg:expr) => {
                match $level {
                    LogLevel::Debug => info!("{}", $msg),
                    LogLevel::Info => info!("{}", $msg),
                    LogLevel::Warning => warn!("{}", $msg),
                    LogLevel::Error => error!("{}", $msg),
                    LogLevel::Fatal => error!("{}", $msg),
                }
                let _ = log_tx_clone
                    .send(BuildLogEntry {
                        level: $level.into(),
                        message: $msg,
                        timestamp: chrono::Utc::now().timestamp(),
                        is_finished: true,
                    })
                    .inspect_err(|e| {
                        error!("Failed to send final log entry: {}", e);
                    });
            };
        }

        let res = async {
                let build_log_filename_suffix = format!("build-{}.log", &build_id_clone);
                // First, check if repo command is available
                if build_settings.do_repo_sync {
                    send_log!(LogLevel::Debug, "Checking for 'repo' command availability...".to_string());
                    if !runner.program_available("repo") {
                        return Err(BuildError::MissingTool("repo".into()));
                    }
                    send_log!(LogLevel::Debug, "'repo' command is available.".to_string());

                    // Open .repo/manifest git repository and check URL and branch
                    let mut need_reinit = false;
                    let manifest_repo_path = &build_dir_clone.join(".repo").join("manifests.git");
                    if fs.exists(&manifest_repo_path) {
                        send_log!(LogLevel::Debug, format!("Opening manifest git repository at {:?}", manifest_repo_path));
                        let repo = git.open(&manifest_repo_path, "origin", None, None)
                            .map_err(|e| {
                                BuildError::internal(format!(
                                    "Failed to open manifest git repository: {}",
                                    e
                                ))
                            })?;

                        let repo_url = repo.get_remote_url().map_err(|x| {
                            BuildError::internal(format!("Cannot retrieve remote-url: {}", x))
                        })?;
                        let branch_name = repo.get_branch_name().map_err(|x| {
                            BuildError::internal(format!("Cannot retrieve branch name: {}", x))
                        })?;

                        send_log!(LogLevel::Debug, format!("manifest repo url:{} branch:{}", repo_url, branch_name));

                        // On a valid repo init result, the .repo/manifests.git should match the expected URL *BUT* branch name is always 'default'
                        // So we have to check the HEAD and remote's branch named "<rom_branch_entry.branch>"
                        if repo_url.trim_end_matches('/') != rom_entry.link.trim_end_matches('/') || !repo.cmp_head_with_remote_branch(&rom_branch_entry.branch).unwrap_or(false) {
                            send_log!(LogLevel::Info, format!(
                                "Manifest repo URL or branch mismatch. Expected URL: {}, branch: {}. Found URL: {}, branch: {}. Will re-initialize repo.",
                                rom_entry.link, rom_branch_entry.branch, repo_url, branch_name
                            ));
                            // Set flag to re-init repo
                            need_reinit = true;
                        }
                    } else {
                        send_log!(LogLevel::Info, format!(
                            "No manifest git repository found at {:?}. Will initialize repo.",
                            manifest_repo_path
                        ));
                        // Set flag to re-init repo
                        need_reinit = true;
                    }

                    if need_reinit {
                        send_log!(LogLevel::Info, "Initializing repo...".to_string());

                        let mut repo_init_cmd = Command::new("repo");
                        repo_init_cmd.arg("init");
                        repo_init_cmd.arg("-u");
                        repo_init_cmd.arg(&rom_entry.link);
                        repo_init_cmd.arg("-b");
                        repo_init_cmd.arg(&rom_branch_entry.branch);
                        repo_init_cmd.arg("--git-lfs");
                        repo_init_cmd.arg("--depth=1");
                        repo_init_cmd.current_dir(&build_dir_clone);
                        BuildService::configure_repo_command_env(
                            &mut repo_init_cmd,
                            &build_dir_clone,
                            askpass_path_clone.as_deref(),
                        );

                        let (stdin_tx, stdin_rx) = mpsc::channel(10);

                        // Sometimes, repo init may ask to "enable colored output" --- we auto-confirm it.
                        stdin_tx
                            .send("y".into())
                            .await
                            .map_err(|e| BuildError::internal(format!("Failed to send to stdin: {}", e)))?;

                        let error_file = (&tempdir_clone).join(format!("{}-{}", "repo-init", &build_log_filename_suffix));
                        info!("Repo init output log path: {:?}", &error_file);

                        let res = run_process(
                            runner.as_ref(),
                            repo_init_cmd,
                            &log_tx_clone,
                            Some(&mut kill_rx),
                            Some(error_file.clone()),
                            stdin_rx.into(),
                        ).await?;
                        if !res {
                            // Update known builds entry to contain failure
                            let known_builds_self = &mut registry.known.lock().await;
                            if let Some(build_entry) = known_builds_self.iter_mut().find(|b| b.id == build_id_clone) {
                                let content = fs.read_to_string(&error_file).unwrap_or_else(|_| "Failed to read error log.".to_string());
                                // Remove error log file after reading
                                fs.remove_file(&error_file).unwrap_or_else(|e| {
                                    error!("Failed to remove error log file {:?}: {}", &error_file, e);
                                });
                                build_entry.success = BuildStatus::Failed(content);
                            }
                            return Err(BuildError::internal("'repo init' command failed or was cancelled."));
                        }
                    }

                    // Prepare local manifest
                    send_log!(LogLevel::Info, "Preparing local manifest...".to_string());
                    // Switches to two cases: One with full prebuilt manifest url, one that we have to write it ourselves.
                    let local_manifest_dir = &build_dir_clone.join(".repo").join("local_manifests");
                    match config_entry {
                        ConfigType::Standard(rom) => {
                            match git.open(
                                &local_manifest_dir,
                                "origin",
                                req.github_token.clone(),
                                None,
                            ) {
                                Ok(repo) => {
                                    if repo.get_remote_url().map_err(|e| {
                                        BuildError::internal(format!(
                                            "Failed to get remote URL of local manifest repository: {}",
                                            e
                                        ))
                                    })? != rom.url
                                    {
                                        send_log!(LogLevel::Warning, "Local manifest repository URL mismatch, re-cloning...".to_string());
                                        fs.remove_dir_all(&local_manifest_dir).map_err(|e| {
                                            BuildError::internal(format!(
                                                "Failed to remove existing local manifest directory: {}",
                                                e
                                            ))
                                        })?;

                                        git.clone_repo(
                                            &rom.url,
                                            &branch_entry.name,
                                            None,
                                            &PathBuf::from(local_manifest_dir),
                                            req.github_token.clone(),
                                            &None,
                                        )
                                        .map_err(|e| {
                                            BuildError::internal(format!(
                                                "Failed to clone local manifest repository: {}",
                                                e
                                            ))
                                        })?;
                                    } else {
                                        send_log!(LogLevel::Info, "Local manifest repository URL matches expected URL.".to_string());
                                        if &repo.get_branch_name().map_err(|e| {
                                            BuildError::internal(format!(
                                                "Failed to get branch name of local manifest repository: {}",
                                                e
                                            ))
                                        })? != &branch_entry.name {
                                            send_log!(LogLevel::Warning, "Local manifest repository branch mismatch, checking out correct branch...".to_string());
                                            repo.checkout_branch(&branch_entry.name).map_err(|e| {
                                                BuildError::internal(format!(
                                                    "Failed to checkout branch {} of local manifest repository: {}",
                                                    &branch_entry.name, e
                                                ))
                                            })?;
                                        } else {
                                            send_log!(LogLevel::Info, String::from("Local manifest repository branch matches expected branch."));
                                        }
                                    send_log!(LogLevel::Info, String::from("Fast-forwarding local manifest repository..."));
                                    let _ = repo
                                        .fast_forward()
                                        .inspect_err(|e| {
                                            send_log!(LogLevel::Error, format!("Failed to fast-forward: {}", e));
                                    });
                                    }
                                }
                                Err(_) => {
                                    send_log!(LogLevel::Info, format!("Cloning local manifest repository from {}...", &rom.url));
                                    git.clone_repo(
                                        &rom.url,
                                        &branch_entry.name,
                                        None,
                                        &PathBuf::from(local_manifest_dir),
                                        req.github_token.clone(),
                                        &None,
                                    )
                                    .map_err(|e| {
                                        BuildError::internal(format!(
                                            "Failed to clone local manifest repository: {}",
                                            e
                                        ))
                                    })?;
                                }
                            }

                            // Handle custom attribute: recurse_submodules:bool on the manifest entry
                            for path in fs.read_dir(&local_manifest_dir).map_err(|e| {
                                BuildError::internal(format!(
                                    "Failed to read local manifests directory: {}",
                                    e
                                ))
                            })?
                            {
                                if path.extension().and_then(|s| s.to_str()) == Some("xml") {
                                    // Parse XML to check for recurse_submodules attribute
                                    let content = fs.read_to_string(&path).map_err(|e| {
                                        BuildError::internal(format!(
                                            "Failed to read local manifest file {:?}: {}",
                                            path, e
                                        ))
                                    })?;
                                    let parser = xml::reader::EventReader::from_str(&content);
                                    for e in parser {
                                        match e {
                                            Ok(xml::reader::XmlEvent::StartElement { name, attributes, .. }) if name.local_name == "project" => {
                                                for attr in &attributes {
                                                    if attr.name.local_name == "recurse_submodules" && attr.value == "true" {
                                                        send_log!(LogLevel::Info, format!("Found recurse_submodules=true in manifest file {:?}, updating submodules...", path));

                                                        let sub_repo_name = attributes.iter()
                                                                .find(|attr| attr.name.local_name == "name").map(|attr| attr.value.clone());
                                                        if sub_repo_name.is_none() {
                                                            send_log!(LogLevel::Warning, format!("recurse_submodules=true is set but no project path found in manifest file {:?}, skipping submodule update.", path));
                                                            continue;
                                                        }
                                                        let sub_repo_path = attributes.iter()
                                                                .find(|attr| attr.name.local_name == "path").map(|attr| attr.value.clone());
                                                        if sub_repo_path.is_none() {
                                                            send_log!(LogLevel::Warning, format!("recurse_submodules=true is set but no project path found in manifest file {:?}, skipping submodule update.", path));
                                                            continue;
                                                        }

                                                        let sub_repo_name = sub_repo_name.unwrap();
                                                        let sub_repo_path = sub_repo_path.unwrap();

                                                        // Perform a partial repo sync for the submodule to initialize the .git directory, then we can use git commands to update submodules.
                                                        let mut repo_sync_command = Command::new("repo");
                                                        repo_sync_command.args([
                                                                "sync",
                                                                "-c",
                                                                "--force-sync",
                                                                "--no-clone-bundle",
                                                                "--no-tags",
                                                                format!("-j{}", parallel_jobs.to_string()).as_str(),
                                                                &(&sub_repo_name.clone()),
                                                            ])
                                                            .current_dir(&build_dir_clone);
                                                        BuildService::configure_repo_command_env(
                                                            &mut repo_sync_command,
                                                            &build_dir_clone,
                                                            askpass_path_clone.as_deref(),
                                                        );
                                                        let error_file = (&tempdir_clone).join(format!("{}-{}-submodule-sync.log", "repo-sync", &build_id_clone));
                                                        info!("Repo sync for submodule output log path: {:?}", &error_file);
                                                        let repo_sync_status = run_process(
                                                            runner.as_ref(),
                                                            repo_sync_command,
                                                            &log_tx_clone,
                                                            Some(&mut kill_rx),
                                                            Some(error_file.clone()),
                                                            None,
                                                        ).await?;
                                                        if !repo_sync_status {
                                                            send_log!(LogLevel::Warning, format!("'repo sync' for submodule {} failed or was cancelled.", &sub_repo_name));
                                                            continue;
                                                        }

                                                        let sub_repo = git.open(
                                                            &PathBuf::from(build_dir_clone.join(&sub_repo_path)),
                                                            "origin",
                                                            req.github_token.clone(),
                                                            None,
                                                        ).map_err(|e| {
                                                            BuildError::internal(format!(
                                                                "Failed to open local manifest git repository for submodule update: {}",
                                                                e
                                                            ))
                                                        })?;
                                                        sub_repo.update_modules().map_err(|e| {
                                                            send_log!(LogLevel::Warning, format!("Git submodule command failed with status: {:?}", e));
                                                            BuildError::internal(format!(
                                                                "Failed to update git submodules: {}",
                                                                e
                                                            ))
                                                        })?;
                                                    }
                                                }
                                            }
                                            Err(e) => {
                                                send_log!(LogLevel::Error, format!("Error parsing XML in manifest file {:?}: {}", path, e));
                                            }
                                            _ => {}
                                        }
                                    }
                                }
                            }
                        }
                        ConfigType::Recovery(recov) => {
                            if !fs.exists(&local_manifest_dir) {
                                fs.create_dir_all(&local_manifest_dir).map_err(|e| {
                                    BuildError::internal(format!(
                                        "Failed to create local manifests directory: {}",
                                        e
                                    ))
                                })?;
                            }
                            let local_manifest_path = local_manifest_dir.join("rombuilder-rs.xml");
                            let mut xml_doc = xml::writer::EmitterConfig::new()
                                .perform_indent(true)
                                .create_writer(std::fs::File::create(&local_manifest_path).map_err(
                                    |e| {
                                        BuildError::internal(format!(
                                            "Failed to create local manifest file: {}",
                                            e
                                        ))
                                    },
                                )?);
                            xml_doc
                                .write(XmlEvent::StartDocument {
                                    version: xml::common::XmlVersion::Version10,
                                    encoding: Some("UTF-8"),
                                    standalone: Some(true),
                                })
                                .map_err(|e| {
                                    BuildError::internal(format!(
                                        "Failed to write XML start document: {}",
                                        e
                                    ))
                                })?;

                            // Scheme:
                            // <manifest>
                            // <remote name="cppbot_github" fetch="https://github.com"/>
                            // <project name="recovery_manifest_name" path="." revision="branch_name"/>
                            // </manifest>
                            xml_doc
                                .write(XmlEvent::start_element("manifest"))
                                .map_err(|e| {
                                    BuildError::internal(format!(
                                        "Failed to write XML start element: {}",
                                        e
                                    ))
                                })?;
                            xml_doc
                                .write(
                                    XmlEvent::start_element("remote")
                                        .attr("name", "cppbot_github")
                                        .attr("fetch", "https://github.com"),
                                )
                                .map_err(|e| {
                                    BuildError::internal(format!(
                                        "Failed to write XML start element: {}",
                                        e
                                    ))
                                })?;
                            xml_doc
                                .write(XmlEvent::end_element()) // remote
                                .map_err(|e| {
                                    BuildError::internal(format!(
                                        "Failed to write XML end element: {}",
                                        e
                                    ))
                                })?;

                            for recov in &recov.clone_mappings {
                                xml_doc
                                    .write(
                                        XmlEvent::start_element("project")
                                            .attr("name", &recov.repo)
                                            .attr("path", &recov.path)
                                            .attr("revision", &recov.branch)
                                            .attr("remote", "cppbot_github"),
                                    )
                                    .map_err(|e| {
                                        BuildError::internal(format!(
                                            "Failed to write XML start element: {}",
                                            e
                                        ))
                                    })?;
                                xml_doc
                                    .write(XmlEvent::end_element()) // project
                                    .map_err(|e| {
                                        BuildError::internal(format!(
                                            "Failed to write XML end element: {}",
                                            e
                                        ))
                                    })?;
                            }
                            xml_doc
                                .write(XmlEvent::end_element()) // manifest
                                .map_err(|e| {
                                    BuildError::internal(format!(
                                        "Failed to write XML end element: {}",
                                        e
                                    ))
                                })?;
                            send_log!(LogLevel::Info, format!("Written local manifest to {:?}", local_manifest_path));
                        }
                    }

                    // Perform repo sync
                    send_log!(LogLevel::Info, "Performing 'repo sync'...".to_string());
                    let mut repo_sync_command = Command::new("repo");
                    repo_sync_command.args([
                            "sync",
                            "-c",
                            "--force-sync",
                            "--no-clone-bundle",
                            "--no-tags",
                            format!("-j{}", parallel_jobs.to_string()).as_str(),
                        ])
                        .current_dir(&build_dir_clone);
                    if force_checkout {
                        repo_sync_command.arg("--force-remove-dirty");
                    }
                    BuildService::configure_repo_command_env(
                        &mut repo_sync_command,
                        &build_dir_clone,
                        askpass_path_clone.as_deref(),
                    );
                    let error_file = (&tempdir_clone).join(format!("{}-{}", "repo-sync", &build_log_filename_suffix));
                    info!("Repo sync output log path: {:?}", &error_file);

                    let repo_sync_status = run_process(
                        runner.as_ref(),
                        repo_sync_command,
                        &log_tx_clone,
                        Some(&mut kill_rx),
                        Some(error_file.clone()),
                        None,
                    ).await?;
                    if !repo_sync_status {
                        send_log!(LogLevel::Info, "'repo sync' command failed or was cancelled.".to_string());
                        // Update known builds entry to contain failure
                        let known_builds_self = &mut registry.known.lock().await;
                        if let Some(build_entry) = known_builds_self.iter_mut().find(|b| b.id == build_id_clone) {
                            let content = fs.read_to_string(&error_file).unwrap_or_else(|_| "Failed to read error log.".to_string());
                            // Remove error log file after reading
                            fs.remove_file(&error_file).unwrap_or_else(|e| {
                                error!("Failed to remove error log file {:?}: {}", &error_file, e);
                            });
                            build_entry.success = BuildStatus::Failed(content);
                        }
                        return Err(BuildError::internal("'repo sync' command failed."));
                    }
                    send_log!(LogLevel::Info, "'repo sync' completed successfully.".to_string());
                };

                #[cfg(unix)]
                {
                    // Get current nofile limits
                    use nix::sys::resource::{getrlimit, Resource};
                    let (soft_limit, hard_limit) = getrlimit(
                        Resource::RLIMIT_NOFILE,
                    ).map_err(|e| {
                        BuildError::internal(format!("Failed to get nofile limit: {}", e))
                    })?;

                    send_log!(LogLevel::Info, format!("Current nofile limits - soft: {}, hard: {}", soft_limit, hard_limit));

                    // AOSP requires us to have at least 16000, but why not 65536?
                    let aosp_soft_limit = 65536;
                    let new_hard_limit = std::cmp::max(hard_limit, aosp_soft_limit);
                    let new_soft_limit = std::cmp::max(aosp_soft_limit, soft_limit);
                    if new_soft_limit == soft_limit && new_hard_limit == hard_limit {
                        send_log!(LogLevel::Info, "Current nofile limits meet AOSP requirements, no need to change.".to_string());
                    } else {
                        send_log!(LogLevel::Info, format!("Setting nofile limits - soft: {}, hard: {}", new_soft_limit, new_hard_limit));
                        let rlim = rlimit {
                            rlim_cur: new_soft_limit,
                            rlim_max: new_hard_limit,
                        };
                        unsafe {
                            use nix::sys::resource::Resource;

                            if setrlimit(Resource::RLIMIT_NOFILE as u32, &rlim) != 0 {
                                return Err(BuildError::internal("Failed to set nofile limit."));
                            }
                        }
                        send_log!(LogLevel::Info, "Successfully set nofile limits.".to_string());
                    }
                }

                // Now, start the build process
                send_log!(LogLevel::Info, "Starting build process...".to_string());

                let use_ccache = build_settings.use_ccache;
                let use_rbe = build_settings.use_rbe_service;

                if use_rbe {
                    send_log!(LogLevel::Info, "Writing RBE environment configuration...".to_string());
                    BuildService::setup_rbe_env(
                        fs.as_ref(),
                        build_dir_clone.clone(),
                        build_settings.rbe_api_token.as_deref().unwrap_or(""),
                    ).await.map_err(|e| {
                        BuildError::internal(format!("Failed to write RBE environment configuration: {}", e))
                    })?;
                    send_log!(LogLevel::Info, "RBE environment configuration written successfully.".to_string());
                }

                // Detect vendor type.
                let vendor_dir = build_dir_clone.join("vendor");
                // Check vendor/<vendor>/config/BoardConfigSoong.mk for known vendors
                let mut vendor_name : String = "unknown".to_string();
                for dir in fs.read_dir(&vendor_dir).map_err(|e| {
                    BuildError::internal(format!("Failed to read vendor directory: {}", e))
                })? {
                    // Check for config/BoardConfigSoong.mk. This requires Android 10+, but most ROMs now target that or higher.
                    // Android 9 is released on August 2018, (that was 8 years ago) so it's reasonable to assume most builds are Android 10+ now.
                    // One exception may be TWRP, but you cannot build old versions of TWRP with this system anyway. (Need docker images with old toolchains)
                    let config_path = dir.join("config").join("BoardConfigSoong.mk");
                    if fs.exists(&config_path) {
                        vendor_name = dir
                            .file_name()
                            .unwrap_or_default()
                            .to_string_lossy()
                            .to_string();
                        send_log!(LogLevel::Info, format!("Detected vendor: {}", vendor_name));
                        break;
                    }
                }
                if vendor_name == "unknown" {
                    send_log!(LogLevel::Warning, "Could not detect vendor from vendor directory.".to_string());
                    vendor_name = "lineage".to_string(); // Default to lineage
                    send_log!(LogLevel::Info, format!("Defaulting to vendor: {} for build.", vendor_name));
                } else {
                    send_log!(LogLevel::Info, format!("Using vendor: {} for build.", vendor_name));
                }

                // Detect build release
                let mut release: Option<String> = None;
                // We have two places to look for build release: build/release and vendor/<vendor>/build/release.
                for path in [&build_dir_clone.join("build").join("release"), &build_dir_clone.join("vendor").join(&vendor_name).join("build").join("release")] {
                    // First, check release_config_map.textproto. Refer: https://android.googlesource.com/platform/build/release/+/925b392ae2a5d212904adec6cfebf4d1b8f574d9.
                    let release_config_map_path = path.join("release_config_map.textproto");
                    if fs.exists(&release_config_map_path) {
                        // In this case, aosp_current is automatically mapped to the latest release.
                        send_log!(LogLevel::Info, format!("Detected release from release_config_map.textproto"));
                        release = Some("aosp_current".to_string());
                        break;
                    }

                    // Check build/release/build_config, scl for Android 14
                    let _build_config_path = path.join("build_config");
                    if fs.is_dir(&_build_config_path) {
                        for file in fs.read_dir(&path).map_err(|e| {
                            BuildError::internal(format!("Failed to read release directory: {}", e))
                        })? {
                            if file.extension().and_then(|s| s.to_str()) == Some("scl") {
                                let file_name = file.file_name().unwrap_or_default().to_string_lossy().to_string();
                                let release_name = file_name.trim_end_matches(".scl");
                                send_log!(LogLevel::Info, format!("Detected release from build_config scl file: {}", release_name));
                                release = Some(release_name.to_string());
                                break; // Use the first valid one
                            }
                        }
                    }

                    // Fallback to build/release/release_configs, textproto, another stuff added on android 15
                    let release_configs_path = path.join("release_configs");
                    if fs.is_dir(&release_configs_path) {
                        let _latest_release: Option<String> = None;
                        for file in fs.read_dir(&release_configs_path).map_err(|e| {
                            BuildError::internal(format!("Failed to read release_configs directory: {}", e))
                        })? {
                            if file.extension().and_then(|s| s.to_str()) == Some("textproto") {
                                let file_name = file.file_name().unwrap_or_default().to_string_lossy().to_string();
                                let release_name = file_name.trim_end_matches(".textproto");
                                if vec!["root", "trunk"].contains(&release_name) {
                                    continue;
                                }
                                send_log!(LogLevel::Info, format!("Detected release from release_configs textproto file: {}", release_name));
                                release = Some(release_name.to_string());
                                break; // Use the first valid one
                            }
                        }
                    }
                };
                if release.is_none() {
                    send_log!(LogLevel::Warning, "Could not detect release from build/release directory.".to_string());
                } else {
                    send_log!(LogLevel::Info, format!("Using release: {} for build.", release.as_ref().unwrap()));
                }

                let build_variant = req.variant.to_string();

                if build_settings.do_clean_build {
                    send_log!(LogLevel::Info, "Performing clean build...".to_string());
                    let out_dir = build_dir_clone.join("out");
                    if fs.exists(&out_dir) {
                        send_log!(LogLevel::Info, format!("Removing output directory at {:?}", out_dir));
                        fs.remove_dir_all(&out_dir).map_err(|e| {
                            BuildError::internal(format!(
                                "Failed to remove output directory for clean build: {}",
                                e
                            ))
                        })?;
                    }
                }

                // Delete matching artifact from previous builds to prevent confusion. We will check for the artifact after the build completes,
                // if it's not there, we know the build failed. If it's there, we know the build succeeded and we can proceed to upload.
                match BuildService::find_artifact(fs.as_ref(), &build_dir_clone, &device_entry.codename, &rom_entry).await {
                    Ok(artifacts) => {
                        send_log!(LogLevel::Info, format!("Removing existing artifact from previous builds to prevent confusion. (count: {})", artifacts.len()));
                        for artifact in artifacts {
                            fs.remove_file(&artifact).map_err(|e| {
                                BuildError::internal(format!(
                                    "Failed to remove existing artifact for clean build: {}",
                                    e
                                ))
                            })?;
                        }
                    }
                    Err(e) => {
                        send_log!(LogLevel::Warning, format!("Failed to check for existing artifacts before build: {}", e));
                    }
                }

                let mut cmd = Command::new("bash");
                cmd.current_dir(&build_dir_clone);
                BuildService::configure_repo_command_env(
                    &mut cmd,
                    &build_dir_clone,
                    askpass_path_clone.as_deref(),
                );

                let (stdin_tx, stdin_rx) = mpsc::channel(100);

                let mut command_list = Vec::new();
                command_list.push("set -e".to_string());
                if use_rbe {
                    command_list.push(format!("source {}", build_dir_clone.join("rbe_env.sh").to_string_lossy()));
                }
                if !use_ccache {
                    command_list.push("unset USE_CCACHE; unset CCACHE_EXEC;".to_string());
                }
                command_list.push("source build/envsetup.sh".to_string());

                // These values originate from on-disk config and repo-synced
                // filenames, so shell-quote every interpolated component before
                // feeding it to bash. Adjacent quoted/unquoted tokens still
                // concatenate into the expected lunch combo argument.
                let command = match release {
                    Some(ref rel) => {
                        format!("lunch {}_{}-{}-{}",
                            BuildService::shell_single_quote(vendor_name.as_str()),
                            BuildService::shell_single_quote(device_entry.codename.as_str()),
                            BuildService::shell_single_quote(rel.as_str()),
                            BuildService::shell_single_quote(&build_variant))
                    }
                    None => {
                        format!("lunch {}_{}-{}",
                            BuildService::shell_single_quote(vendor_name.as_str()),
                            BuildService::shell_single_quote(device_entry.codename.as_str()),
                            BuildService::shell_single_quote(&build_variant))
                    }
                };
                command_list.push(command);
                command_list.push(format!("m {} -j{}",
                    BuildService::shell_single_quote(rom_entry.target.as_str()),
                    parallel_jobs));
                command_list.push("exit 0".to_string());

                for line in command_list {
                    stdin_tx.send((&line).clone().into()).await.map_err(|e| BuildError::internal(format!("Failed to send to stdin: {}", e)))?;
                    send_log!(LogLevel::Info, format!("Sent to stdin: {}", line));
                }

                let error_file_path = (&tempdir_clone).join(format!("{}-{}", "build-output", &build_log_filename_suffix));
                if !run_process(
                    runner.as_ref(),
                    cmd,
                    &log_tx_clone,
                    Some(&mut kill_rx),
                    Some(error_file_path.clone()),
                    stdin_rx.into(),
                ).await? {
                    send_log!(LogLevel::Error, "Build command failed".to_string());
                    // Update known builds entry to contain failure
                    let known_builds_self = &mut registry.known.lock().await;
                    if let Some(build_entry) = known_builds_self.iter_mut().find(|b| b.id == build_id_clone) {
                        let error_file = (&build_dir_clone).join("out").join("error.log");
                        send_log!(LogLevel::Info, format!("Reading error log from {:?}", error_file));
                        let content = fs.read_to_string(&error_file).unwrap_or_else(|_| "Failed to read error log.".to_string());
                        if content.is_empty() {
                            send_log!(LogLevel::Info, format!("Error log is empty, reading from build output log at {:?}", error_file_path));
                            let content = fs.read_to_string(&error_file_path).unwrap_or_else(|_| "Failed to read error log.".to_string());
                            if content.is_empty() {
                                send_log!(LogLevel::Warning, "Build output log is also empty, setting generic error message.".to_string());
                                build_entry.success = BuildStatus::Failed("Build failed, but no error log available.".to_string());
                            } else {
                                build_entry.success = BuildStatus::Failed(content);
                            }
                        } else {
                            build_entry.success = BuildStatus::Failed(content);
                        }
                    }
                    return Err(BuildError::internal("Build command failed or was cancelled."));
                }
                send_log!(LogLevel::Info, "Build process completed successfully.".to_string());

                if build_settings.do_upload {
                    match BuildService::find_artifact(fs.as_ref(), &build_dir_clone, &device_entry.codename, &rom_entry).await {
                        Ok(artifact) => {
                        send_log!(LogLevel::Info, format!("Found {} artifact(s) for upload.", artifact.len()));
                        for (i, path) in artifact.iter().enumerate() {
                            send_log!(LogLevel::Info, format!("Artifact {}: {:?}", i + 1, path));
                        }
                        if artifact.len() != 1 {
                            send_log!(LogLevel::Warning, format!("Expected to find exactly one artifact, but found {}.", artifact.len()));
                        }
                        let artifact_it = artifact.into_iter().next().unwrap_or_else(|| {
                            send_log!(LogLevel::Error, "Failed to select an artifact for upload.".to_string());
                            std::path::PathBuf::new()
                        });
                        send_log!(LogLevel::Info, format!("Build artifact located at: {:?}", artifact_it));
                        let artifact = BuildArtifact::new(artifact_it, artifact_kind)
                            .map_err(BuildError::internal)?;
                        let mut uploads = registry.uploads.lock().await;
                        match req.upload_method {
                            RomUploadMethod::None => {
                                send_log!(LogLevel::Info, "Upload method set to None, skipping upload.".to_string());
                            }
                            RomUploadMethod::LocalFile => {
                                send_log!(LogLevel::Info, format!("File ready at local path: {:?}", artifact.path));
                                uploads.push(
                                    UploadTask {
                                        build_id: build_id_clone.clone(),
                                        method: RomUploadMethod::LocalFile,
                                        artifact: artifact.clone(),
                                    }
                                );
                            }
                            RomUploadMethod::GoFile => {
                                send_log!(LogLevel::Info, "Scheduling upload to GoFile...".to_string());
                                uploads.push(
                                    UploadTask {
                                        build_id: build_id_clone.clone(),
                                        method: RomUploadMethod::GoFile,
                                        artifact: artifact.clone(),
                                    }
                                );
                            }
                            RomUploadMethod::Stream => {
                                send_log!(LogLevel::Info, "Scheduling upload to Stream...".to_string());
                                uploads.push(
                                    UploadTask {
                                        build_id: build_id_clone.clone(),
                                        method: RomUploadMethod::Stream,
                                        artifact,
                                    }
                                );
                            }
                            _ => {
                                send_log!(LogLevel::Warning, "Unknown upload method specified, skipping upload.".to_string());
                            }
                        }

                        }
                        Err(e) => {
                            send_log!(LogLevel::Error, format!("Failed to find build artifact: {}", e));
                        }
                    }
                } else {
                    send_log!(LogLevel::Info, "Upload disabled in settings, skipping artifact search.".to_string());
                };

                // Mark build as successful in known builds
                let known_builds_self = &mut registry.known.lock().await;
                if let Some(build_entry) = known_builds_self.iter_mut().find(|b| b.id == build_id_clone) {
                    build_entry.success = BuildStatus::Success;
                }
                Ok(())
            }.instrument(span);

        match res.await {
            Ok(_) => {
                send_log_final!(
                    LogLevel::Info,
                    String::from("Build completed successfully.")
                );
            }
            Err(e) => {
                // Update known builds entry to contain failure
                let known_builds_self = &mut registry.known.lock().await;
                if let Some(build_entry) = known_builds_self
                    .iter_mut()
                    .find(|b| b.id == build_id_clone)
                {
                    if let BuildStatus::Failed(msg) = &build_entry.success {
                        // Already have an error message, do not overwrite. Just append.
                        build_entry.success = BuildStatus::Failed(format!(
                            "{}\nWhich caused builder error: {}",
                            msg, e
                        ));
                    } else {
                        build_entry.success = BuildStatus::Failed(format!("{}", e));
                    }
                }
                send_log_final!(LogLevel::Error, format!("Build failed: {}", e));
            }
        }

        // Cleanup when done
        let mut lock = registry.active.lock().await;
        *lock = None;

        Ok(())
    }
}
