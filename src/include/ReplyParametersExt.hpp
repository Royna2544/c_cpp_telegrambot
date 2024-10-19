#pragma once

#include <tgbot/types/ReplyParameters.h>

#include <memory>

#include "Types.h"

// Extension of ReplyParameters
struct ReplyParametersExt : public TgBot::ReplyParameters {
    using Ptr = std::shared_ptr<ReplyParametersExt>;
    static constexpr MessageThreadId kThreadIdNone = 0;

    MessageThreadId messageThreadId{};

    [[nodiscard]] bool hasThreadId() const {
        return messageThreadId != kThreadIdNone;
    }
};