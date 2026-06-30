#include "SystemStatsProvider.hpp"

#include <absl/log/log.h>
#include <google/protobuf/empty.pb.h>
#include <grpcpp/client_context.h>

namespace tgbot::builder {

SystemSnapshot SystemStatsProvider::snapshot(const std::string& diskPath) const {
    SystemSnapshot out;
    if (!stub_) {
        return out;
    }

    system_monitor::SystemStats stats;
    {
        grpc::ClientContext context;
        auto status =
            stub_->GetStats(&context, ::google::protobuf::Empty{}, &stats);
        if (!status.ok()) {
            LOG(ERROR) << "GetStats RPC failed: " << status.error_message();
            return out;
        }
    }

    system_monitor::SystemInfo info;
    {
        grpc::ClientContext context;
        system_monitor::GetSystemInfoRequest request;
        request.set_disk_path(diskPath);
        auto status = stub_->GetSystemInfo(&context, request, &info);
        if (!status.ok()) {
            LOG(ERROR) << "GetSystemInfo RPC failed: " << status.error_message();
            // Stats alone are still useful; report what we have.
        }
    }

    out.valid = true;
    out.cpuName = info.cpu_name();
    out.cpuUsagePercent = stats.cpu_usage_percent();
    out.memUsedMb = stats.memory_used_mb();
    out.memTotalMb = stats.memory_total_mb();
    out.diskUsedGb = info.disk_used_gb();
    out.diskTotalGb = info.disk_total_gb();
    return out;
}

}  // namespace tgbot::builder
