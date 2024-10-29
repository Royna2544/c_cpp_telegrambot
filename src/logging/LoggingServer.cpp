#include "LoggingServer.hpp"

#include <CStringLifetime.h>
#include <absl/log/log_entry.h>
#include <absl/log/log_sink.h>
#include <absl/log/log_sink_registry.h>

#include <ManagedThreads.hpp>
#include <SharedMalloc.hpp>
#include <future>
#include <impl/backends/ServerBackend.hpp>
#include <impl/bot/TgBotSocketFileHelper.hpp>
#include <memory>
#include <mutex>

#include "LogcatData.hpp"
#include "SocketBase.hpp"

void NetworkLogSink::Send(const absl::LogEntry& entry) {
    if (!enabled) {
        return;
    }
    LogEntry le{};
    le.severity = entry.log_severity();
    copyTo(le.message, entry.text_message().data());
    SharedMalloc logData(le);
    bool ret = false;
    {
        std::lock_guard<std::mutex> _(mContextMutex);
        if (context != nullptr) {
            ret = interface->writeToSocket(*context, logData);
        }
    }
    if (!ret) {
        LOG(INFO) << "onClientDisconnected";
        enabled = false;
        onClientDisconnected.set_value();
    }
}

bool NetworkLogSink::socketThreadFunction(SocketConnContext c,
                                          std::shared_future<void> _future) {
    auto future = std::move(_future);
    {
        std::lock_guard<std::mutex> _(mContextMutex);
        context = &c;
    }
    absl::AddLogSink(this);
    isSinkAdded = true;
    future.wait();
    {
        std::lock_guard<std::mutex> _(mContextMutex);
        context = nullptr;
    }
    return true;
}

void NetworkLogSink::runFunction() {
    std::shared_future<void> future = onClientDisconnected.get_future();
    bool isSinkAdded = false;
    std::thread listenThread([this, future]() {
        interface->startListeningAsServer([this, future](SocketConnContext c) {
            return socketThreadFunction(c, future);
        });
    });
    future.wait();
    interface->forceStopListening();
    listenThread.join();
    if (isSinkAdded) {
        absl::RemoveLogSink(this);
    }
}

NetworkLogSink::NetworkLogSink(SocketServerWrapper* wrapper) {
    interface = wrapper->getInternalInterface();
    if (interface) {
        interface->options.address.set(getSocketPathForLogging().string());
        interface->options.port.set(SocketInterfaceBase::kTgBotLogPort);
    } else {
        LOG(ERROR) << "Failed to find default socket interface";
        return;
    }
    setPreStopFunction([](auto* arg) {
        LOG(INFO) << "onServerShutdown";
        auto* const thiz = static_cast<NetworkLogSink*>(arg);
        thiz->enabled = false;
        thiz->onClientDisconnected.set_value();
    });
    run();
}
