# Command security audit

Last reviewed: 2026-08-15

## Scope and threat model

This document covers Telegram command dispatch, dynamically loaded command
modules, LLM/MCP tool execution, native media rendering, process execution,
spam handling, and the SQLite/Protobuf bot databases. Telegram users are
untrusted. Whitelisted administrators are trusted for ordinary administration;
only the configured owner is trusted with process execution, ACL changes,
module lifecycle, spam control, and log export.

The shell/compiler commands intentionally execute directly on the bot host.
They are resource- and queue-bounded, but they are not a sandbox.

## Implemented controls

- `Owner` is distinct from whitelist administration. Module lifecycle, ACL
  mutation, logs, spam, bash/interactive bash, and compiler/interpreter
  commands are owner-only. Both databases reject blacklisting the owner.
- Fast command dispatch remains a two-worker bounded queue. LLM, media,
  process, scheduled outbound, and unlimited `/ubash` work use separate
  bounded lanes. Queue depth, rejection, duration, deadline, cancellation,
  and timeout events are logged.
- Command quota is reserved only after argument validation and successful
  queue admission. Users receive one feedback message per limited window and
  recovery is logged on the first accepted command in a new window.
- Module unload/reload is serialized. Self-unload/reload is rejected;
  listeners, callbacks, and queued work are cancelled and drained while the
  DSO remains mapped. Reload preflights a staged image and keeps the previous
  image mapped until the replacement listener is installed.
- Module-owned any-message work is bounded, per-callback serialized,
  exception-isolated, expiring where applicable, and drained before `dlclose`.
  Callback-query and inline-query invocations also carry an execution context
  which prevents callback-initiated self-unload.
- LLM HTTP has 15-second connect, 30-second low-speed, and 180-second total
  limits, 4 MiB request/response limits, and cooperative cancellation. LLM
  output is split on UTF-8 boundaries for Telegram.
- The capability router exposes only the selected tool set, and the executor
  enforces the same allowlist. A response may contain at most four tool calls,
  a turn at most eight, and duplicate call IDs are rejected before any call in
  that batch runs. Tool results are capped at 16 KiB.
- At most one consequential LLM action may be attempted per turn. Telegram
  sends and registry writes require a single-use approval bound to canonical
  tool name and arguments. Builder tools only stage a plan for their own final
  Telegram review.
- `/q` renders in process and sends no quote content to an external service.
  Asset resolution is injected, input/source/pixel/canvas/output budgets are
  checked, and static-sticker output is validated as bounded WebP.
- Runtime randomness uses per-thread PRNG state seeded from operating-system
  entropy; commands never read a blocking HWRNG stream. `/possibility` is
  bounded and exact-total; `/decide`, `/flash`, and `/spam` no longer sleep a
  fast command worker.
- Process execution preserves exit status, caps captured output, and uses
  process groups on POSIX and a Job Object on Windows. Cancellation and timeout
  use a graceful signal, a two-second grace period, and forced termination.
  `/ubash` has one isolated cancellable job slot.
- Image/video processing runs on the media lane and enforces compressed,
  decoded-pixel, frame, duration, encoded-output, cancellation, and wall-clock
  limits.
- Spam detection swaps its batch under the data lock and performs Telegram
  delete/mute actions after releasing that lock. A slow Telegram request can
  no longer block update dispatch. Action exceptions are isolated.
- SQLite access is serialized and owner claim is atomic with a partial unique
  index. Migration retains the earliest legacy owner and demotes additional
  owners to the whitelist. Protobuf mutations are acknowledged only after a
  flushed temporary snapshot is atomically replaced; memory rolls back on a
  persistence failure.

## Accepted and residual risks

- **Accepted: unsandboxed owner execution.** `/bash`, `/ibash`, compilers, and
  `/ubash` can read or change everything available to the bot service account.
  `/ubash` has no runtime deadline by design. A compromised owner account is
  equivalent to compromise of that service account.
- POSIX process groups contain ordinary descendants, but deliberately hostile
  code running with the same account may attempt OS-specific escape techniques
  such as starting a new session. Strong isolation requires a container,
  seccomp/landlock, a dedicated account, or a cgroup supervisor and is outside
  the selected unsandboxed design.
- Deadlines for in-process C++ work are cooperative. The renderer and image
  codecs have explicit checkpoints, but a third-party codec stuck inside one
  library call cannot be forcefully unwound safely. The isolated media lane
  prevents it from taking both fast command workers.
- A human confirmation can occupy one of the two LLM workers for up to 90
  seconds. It never occupies a fast command or outbound worker; two abandoned
  confirmations can temporarily defer other `/ask` work.
- Telegram HTTP is still synchronous and may serialize outbound requests for
  up to the configured client timeout/retry policy. This affects the outbound
  lane rather than command ingestion.
- Model-picker callbacks are bound to the initiating user, a single current
  picker generation, and a five-minute expiry. The resulting selection is
  intentionally chat-scoped, so a user explicitly running `/ask model ...`
  still changes the model used by later turns in that chat. Per-user model
  preferences would require keying selection by `(chat,user)`.
- Telegram's current HTTP transport materializes a downloaded response in a
  `std::string`. The bot rejects files from `getFile.fileSize` before download
  when Telegram supplies that field and checks the received size afterward,
  but a response whose metadata omits the size can allocate before that final
  cap. Strict pre-allocation enforcement requires a streaming transport API.
- Lua state access is serialized, but Lua commands do not yet install an
  instruction-count hook. A non-terminating owner-installed Lua module can pin
  that module until the process is restarted.
- The fixed-window command limiter is intentionally simple rather than a
  sliding-window/token-bucket limiter. It recovers lazily on the next command;
  there is no background "unlimited" notification.
- SQLite uses one serialized connection. This favors transaction correctness
  over write throughput. Protobuf persistence performs a full snapshot for
  each mutation, favoring durability over throughput.
- Proprietary emoji artwork is not redistributed. Compatible brand selectors
  resolve to the documented bundled open-artwork fallback.

## Operational guidance

- Run the bot as a dedicated unprivileged OS account and keep the owner ID and
  Telegram token out of logs and source control.
- Monitor queue rejection, deadline, cancellation, rejected-tool, render
  duration, and persistence-failure logs. Repeated lane saturation should be
  treated as an availability incident rather than solved by increasing every
  queue.
- Keep the local LLM endpoint bound to loopback or an authenticated private
  network. Tool allowlisting is enforced locally even when the model server is
  trusted.
- Back up the database and test restore/migration before deploying a new
  schema. A Protobuf persistence failure is reported to the caller and should
  be investigated immediately.
