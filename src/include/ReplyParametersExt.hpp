#pragma once

#include <tgbot/types/ReplyParameters.h>

#include <memory>

#include "Types.h"

// Extension of ReplyParameters
struct ReplyParametersExt : public TgBot::ReplyParameters {
    using Ptr = std::shared_ptr<ReplyParametersExt>;

    std::optional<MessageThreadId> messageThreadId;

    [[nodiscard]] bool hasThreadId() const {
        return messageThreadId.has_value();
    }
};