#pragma once

#include <TgBotPPImplExports.h>
#include <absl/log/log_entry.h>
#include <absl/log/log_sink.h>

#include <InitTask.hpp>
#include <ManagedThreads.hpp>
#include <future>
#include <memory>
#include <mutex>

#include "SocketBase.hpp"

struct TgBotPPImpl_API NetworkLogSink : private absl::LogSink,
                                        ManagedThreadRunnable {
    void Send(const absl::LogEntry& entry) override;

    void runFunction() override;

    explicit NetworkLogSink();

    friend InitTask& operator<<(InitTask& tag, NetworkLogSink& thiz);

   private:
    std::shared_ptr<SocketInterfaceBase> interface;
    std::atomic_bool enabled = true;
    std::promise<void> onClientDisconnected;

    std::mutex mContextMutex;  // Protects context
    SocketConnContext* context = nullptr;
};