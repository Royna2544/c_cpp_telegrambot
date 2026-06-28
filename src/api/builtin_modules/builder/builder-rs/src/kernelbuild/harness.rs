use std::{num::NonZero, path::PathBuf, sync::Arc};

use async_trait::async_trait;
use builder::ratelimit::RateLimit;
use tokio::{
    process::Command,
    sync::{Mutex, mpsc},
};
use tonic::Status;

use super::grpc::grpc_pb::{BuildStatus, ProgressStatus};
use crate::build_common::{
    BuildEvent, BuildEventSender, BuildTerminalState, CommandSpec, ProcessRunner,
};

struct KernelProcessEvents {
    tx: mpsc::Sender<Result<BuildStatus, Status>>,
    build_id: Option<i32>,
    ratelimit: Mutex<RateLimit>,
}

#[async_trait]
impl BuildEventSender for KernelProcessEvents {
    async fn send(&self, event: BuildEvent) {
        let (output, cancelled) = match event {
            BuildEvent::Stdout { line, .. } => (format!("stdout: {line}"), false),
            BuildEvent::Stderr { line, .. } => (format!("stderr: {line}"), false),
            BuildEvent::Finished { outcome, .. }
                if outcome.state == BuildTerminalState::Cancelled =>
            {
                ("Build cancelled.".into(), true)
            }
            _ => return,
        };
        if !cancelled && !self.ratelimit.lock().await.check() {
            return;
        }
        let status = if cancelled {
            ProgressStatus::Failed
        } else {
            ProgressStatus::InProgressBuild
        };
        let _ = self
            .tx
            .send(Ok(BuildStatus {
                status: status.into(),
                output,
                build_id: self.build_id,
            }))
            .await;
    }
}

pub(crate) async fn run_process(
    runner: &dyn ProcessRunner,
    command: Command,
    tx: mpsc::Sender<Result<BuildStatus, Status>>,
    build_id: Option<i32>,
    mut kill_rx: Option<mpsc::Receiver<()>>,
    log_path: Option<PathBuf>,
) -> Result<bool, Status> {
    runner
        .run(
            CommandSpec::new(command).with_log_path(log_path),
            Arc::new(KernelProcessEvents {
                tx,
                build_id,
                ratelimit: Mutex::new(RateLimit::new(NonZero::new(3).unwrap())),
            }),
            kill_rx.as_mut(),
        )
        .await
        .map(|outcome| outcome.success)
        .map_err(|error| Status::internal(error.to_string()))
}
