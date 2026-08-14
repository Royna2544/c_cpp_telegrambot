#include <api/CommandModule.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "compiler/CompilerInTelegram.hpp"
#include "compiler/Helper.hpp"

namespace {

constexpr std::string_view kBashOwner = "bash";
constexpr std::string_view kUbashOwner = "ubash";

struct UbashSubmission {
    bool completed = false;
};

struct UbashJobState {
    std::mutex mutex;
    std::optional<TgBotApi::WorkId> active;
    bool submitting = false;
    bool cancellationRequested = false;
};

UbashJobState ubashJob;

struct UbashCompletionGuard {
    std::shared_ptr<UbashSubmission> submission;

    ~UbashCompletionGuard() {
        std::scoped_lock lock(ubashJob.mutex);
        submission->completed = true;
        ubashJob.active.reset();
    }
};

bool isUbashCancel(const MessageExt& message) {
    if (!message.has<MessageAttrs::ExtraText>()) {
        return false;
    }
    const auto text = message.get<MessageAttrs::ExtraText>();
    const auto first = text.find_first_not_of(" \t\r\n\f\v");
    if (first == std::string::npos) {
        return false;
    }
    const auto last = text.find_last_not_of(" \t\r\n\f\v");
    return text.substr(first, last - first + 1) == "cancel";
}

DECLARE_COMMAND_HANDLER(bash) {
    auto ownedMessage = std::make_shared<MessageExt>(*message);
    if (!api->submitCommandWork(
            kBashOwner, TgBotApi::WorkClass::Process,
            [api, res,
             ownedMessage = std::move(ownedMessage)](std::stop_token stop) {
                auto helper = std::make_unique<CompilerInTgBotInterface>(
                    api, res, ownedMessage.get());
                CompilerInTgForBash bash(std::move(helper), res, false);
                bash.run(ownedMessage.get(), stop);
            })) {
        api->sendReplyMessage(message->message(),
                              "The process queue is full. Please retry later.");
    }
}
DECLARE_COMMAND_HANDLER(ubash) {
    if (isUbashCancel(*message)) {
        std::optional<TgBotApi::WorkId> active;
        bool submitting = false;
        {
            std::scoped_lock lock(ubashJob.mutex);
            active = ubashJob.active;
            submitting = ubashJob.submitting;
            if (submitting && !active) {
                ubashJob.cancellationRequested = true;
            }
        }
        const bool requested =
            active && api->cancelCommandWork(kUbashOwner, *active);
        if (active) {
            std::scoped_lock lock(ubashJob.mutex);
            if (ubashJob.active == active) {
                ubashJob.active.reset();
            }
        }
        api->sendReplyMessage(message->message(),
                              requested || submitting
                                  ? "Cancellation requested."
                                  : "No /ubash job is running.");
        return;
    }

    bool alreadyRunning = false;
    {
        std::scoped_lock lock(ubashJob.mutex);
        if (ubashJob.submitting || ubashJob.active) {
            alreadyRunning = true;
        } else {
            ubashJob.submitting = true;
        }
    }
    if (alreadyRunning) {
        api->sendReplyMessage(
            message->message(),
            "Another /ubash job is already running. Use /ubash cancel first.");
        return;
    }

    auto ownedMessage = std::make_shared<MessageExt>(*message);
    auto submission = std::make_shared<UbashSubmission>();
    const auto workId = api->submitCommandWork(
        kUbashOwner, TgBotApi::WorkClass::UnboundedProcess,
        [api, res, ownedMessage = std::move(ownedMessage),
         submission](std::stop_token stop) {
            const UbashCompletionGuard completed{submission};
            auto helper = std::make_unique<CompilerInTgBotInterface>(
                api, res, ownedMessage.get());
            CompilerInTgForBash ubash(std::move(helper), res, true);
            ubash.run(ownedMessage.get(), stop);
        });

    std::optional<TgBotApi::WorkId> cancelAfterSubmit;
    {
        std::scoped_lock lock(ubashJob.mutex);
        ubashJob.submitting = false;
        if (workId && !submission->completed) {
            ubashJob.active = *workId;
            if (ubashJob.cancellationRequested) {
                cancelAfterSubmit = *workId;
            }
        }
        ubashJob.cancellationRequested = false;
    }
    if (cancelAfterSubmit) {
        (void)api->cancelCommandWork(kUbashOwner, *cancelAfterSubmit);
        std::scoped_lock lock(ubashJob.mutex);
        if (ubashJob.active == cancelAfterSubmit) {
            ubashJob.active.reset();
        }
    }
    if (!workId) {
        api->sendReplyMessage(
            message->message(),
            "Another /ubash job is already running. Use /ubash cancel first.");
    }
}

}  // namespace

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::OwnerOnly,
#ifdef cmd_bash_EXPORTS
    .name = "bash",
    .description = "Run bash commands",
    .function = COMMAND_HANDLER_NAME(bash),
#endif
#ifdef cmd_ubash_EXPORTS
    .name = "ubash",
    .description = "Run bash commands w/o timeout",
    .function = COMMAND_HANDLER_NAME(ubash),
#endif
    .valid_args = {}};
