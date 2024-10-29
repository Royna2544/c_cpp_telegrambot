#pragma once

#include <absl/log/log_entry.h>
#include <absl/log/log_sink.h>

#include <ManagedThreads.hpp>
#include <future>
#include <impl/backends/ServerBackend.hpp>
#include <memory>
#include <mutex>

#include "SocketBase.hpp"

struct NetworkLogSink : private absl::LogSink,
                                        ManagedThreadRunnable {
    void Send(const absl::LogEntry& entry) override;

    void runFunction() override;

    explicit NetworkLogSink(SocketServerWrapper* wrapper);
   private:
    std::shared_ptr<SocketInterfaceBase> interface;
    std::atomic_bool enabled = true;
    std::promise<void> onClientDisconnected;

    bool socketThreadFunction(SocketConnContext c, std::shared_future<void> future);
    std::mutex mContextMutex;  // Protects context
    SocketConnContext* context = nullptr;
    bool isSinkAdded = false;
};