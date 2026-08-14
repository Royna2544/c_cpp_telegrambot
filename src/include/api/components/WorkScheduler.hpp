#pragma once

#include <api/TgBotApiImpl.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

class TgBotApiImpl::WorkScheduler {
   public:
    using WorkClass = TgBotApi::WorkClass;
    using WorkId = TgBotApi::WorkId;
    using WorkOptions = TgBotApi::WorkOptions;
    using Work = TgBotApi::CancellableWork;

    WorkScheduler();
    ~WorkScheduler();

    NO_COPY_CTOR(WorkScheduler);

    [[nodiscard]] std::optional<WorkId> submit(
        std::string owner, WorkClass workClass, Work work,
        WorkOptions options = {}, std::shared_ptr<void> moduleLease = {});
    [[nodiscard]] bool cancel(std::string_view owner, WorkId id);
    void cancelAndDrain(std::string_view owner);
    [[nodiscard]] std::size_t depth(WorkClass workClass) const;

   private:
    class Lane;
    Lane& lane(WorkClass workClass);
    const Lane& lane(WorkClass workClass) const;

    std::atomic<WorkId> nextId_{1};
    std::unique_ptr<Lane> llm_;
    std::unique_ptr<Lane> media_;
    std::unique_ptr<Lane> process_;
    std::unique_ptr<Lane> outbound_;
    std::unique_ptr<Lane> unboundedProcess_;
};
