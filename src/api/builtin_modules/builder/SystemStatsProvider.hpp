#pragma once

#include <memory>
#include <string>

#include "BuildObserver.hpp"
#include "SystemMonitor_service.grpc.pb.h"

namespace tgbot::builder {

/**
 * @brief Thin helper that turns the SystemMonitor gRPC service into a plain
 *        @ref SystemSnapshot.
 *
 * This is a presentation-side convenience: orchestrators stay free of system
 * monitoring, while observers (or any caller) can pull a snapshot on demand
 * when they decide to render. Failures yield a snapshot with @c valid == false
 * rather than throwing.
 */
class SystemStatsProvider {
   public:
    explicit SystemStatsProvider(
        std::unique_ptr<system_monitor::SystemMonitorService::Stub> stub)
        : stub_(std::move(stub)) {}

    /// Fetch a combined stats + static-info snapshot. @p diskPath selects the
    /// mount to report disk usage for.
    [[nodiscard]] SystemSnapshot snapshot(const std::string& diskPath = "/") const;

    [[nodiscard]] bool valid() const { return stub_ != nullptr; }

   private:
    std::unique_ptr<system_monitor::SystemMonitorService::Stub> stub_;
};

}  // namespace tgbot::builder
