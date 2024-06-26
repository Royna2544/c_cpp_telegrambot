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
#include <initcalls/Initcall.hpp>
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

void NetworkLogSink::doInitCall() {
    setPreStopFunction([this](auto*) {
        LOG(INFO) << "onServerShutdown";
        enabled = false;
        onClientDisconnected.set_value();
    });
    run();
}

void NetworkLogSink::runFunction() {
    std::shared_future<void> future = onClientDisconnected.get_future();
    bool isSinkAdded = false;
    const auto function = [this, future,
                           &isSinkAdded](SocketConnContext c) -> bool {
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
    };
    std::thread listenThread(
        [this, function]() { interface->startListeningAsServer(function); });
    future.wait();
    interface->forceStopListening();
    listenThread.join();
    if (isSinkAdded) {
        absl::RemoveLogSink(this);
    }
}

const CStringLifetime NetworkLogSink::getInitCallName() const {
    return "Initialize network logsink";
}

NetworkLogSink::NetworkLogSink() {
    SocketServerWrapper wrapper;
    interface = wrapper.getInternalInterface();
    interface->options.address.set(getSocketPathForLogging().string());
    interface->options.port.set(SocketInterfaceBase::kTgBotLogPort);
}
