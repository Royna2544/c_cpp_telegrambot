#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace tgbot::builder {

/**
 * @brief UI- and protocol-agnostic build lifecycle phase.
 *
 * Both the kernel and ROM orchestrators map their protocol-specific status
 * enums onto these semantic phases so observers need not know either proto.
 */
enum class BuildPhase : std::uint8_t {
    Queued,
    Preparing,
    Downloading,
    Configuring,
    Building,
    Uploading,
    Completed,
    Failed,
    Cancelled,
};

[[nodiscard]] constexpr std::string_view to_string(BuildPhase phase) {
    switch (phase) {
        case BuildPhase::Queued:
            return "Queued";
        case BuildPhase::Preparing:
            return "Preparing";
        case BuildPhase::Downloading:
            return "Downloading";
        case BuildPhase::Configuring:
            return "Configuring";
        case BuildPhase::Building:
            return "Building";
        case BuildPhase::Uploading:
            return "Uploading";
        case BuildPhase::Completed:
            return "Completed";
        case BuildPhase::Failed:
            return "Failed";
        case BuildPhase::Cancelled:
            return "Cancelled";
    }
    return "Unknown";
}

/// Snapshot of build-host resource usage. @c valid is false when unavailable.
struct SystemSnapshot {
    bool valid = false;
    std::string cpuName;
    float cpuUsagePercent = 0.0F;
    std::uint64_t memUsedMb = 0;
    std::uint64_t memTotalMb = 0;
    std::int32_t diskUsedGb = 0;
    std::int32_t diskTotalGb = 0;
};

/// A single progress tick emitted by an orchestrator.
struct ProgressEvent {
    BuildPhase phase = BuildPhase::Queued;
    std::string message;          ///< Latest output / log line.
    std::int64_t timestamp = 0;   ///< Unix seconds, 0 if not applicable.
};

/**
 * @brief Sink for build progress, decoupled from any transport or UI.
 *
 * Orchestrators push events here; concrete implementations (e.g. a Telegram
 * presenter, a CLI logger, a test spy) decide how to surface them.
 *
 * Orchestrators do NOT rate-limit: observers that render to an external
 * surface should throttle themselves.
 */
class IBuildObserver {
   public:
    virtual ~IBuildObserver() = default;

    /// Invoked for each progress tick during a streaming phase.
    virtual void onProgress(const ProgressEvent& event) {}

    /// Streamed artifact lifecycle (raw bytes, no framing assumptions).
    virtual void onArtifactMeta(std::string_view filename,
                                std::uint64_t totalSize) {}
    virtual void onArtifactChunk(const char* data, std::size_t size) {}

    /// Terminal outcomes. Exactly one is invoked per orchestrated operation.
    virtual void onCompleted(std::string_view message) {}
    virtual void onFailed(std::string_view message) {}

    /// Polled cooperatively between reads so orchestrators can abort without
    /// any knowledge of how cancellation was requested.
    [[nodiscard]] virtual bool cancelled() { return false; }
};

}  // namespace tgbot::builder
