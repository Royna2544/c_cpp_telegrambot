#include "LoggingServer.hpp"

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
#include <stop_token>
#include <utility>

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
            ret = _interface->writeToSocket(*context, logData);
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

void NetworkLogSink::runFunction(const std::stop_token& token) {
    std::shared_future<void> future = onClientDisconnected.get_future();
    bool isSinkAdded = false;
    std::thread listenThread([this, future]() {
        _interface->startListeningAsServer([this, future](SocketConnContext c) {
            return socketThreadFunction(std::move(c), future);
        });
    });
    future.wait();
    _interface->forceStopListening();
    listenThread.join();
    if (isSinkAdded) {
        absl::RemoveLogSink(this);
    }
}

void NetworkLogSink::onPreStop() {
    enabled = false;
    onClientDisconnected.set_value();
    LOG(INFO) << "onPreStop";
}

NetworkLogSink::NetworkLogSink(SocketServerWrapper* wrapper) {
    _interface = wrapper->getInternalInterface();
    if (_interface) {
        _interface->options.address.set(getSocketPathForLogging().string());
        _interface->options.port.set(SocketInterfaceBase::kTgBotLogPort);
    } else {
        LOG(ERROR) << "Failed to find default socket interface";
        return;
    }
    run();
}
