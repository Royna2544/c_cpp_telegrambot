#pragma once

#include <memory>

#include "api/typedefs.h"
#include "api/types/TelegramTypes.hpp"

// Extension of ReplyParameters for our project
class ReplyParametersExt : public tgbot_api::ReplyParameters {
   public:
    using Ptr = ReplyParametersExt*;

    [[nodiscard]] bool hasThreadId() const {
        return messageThreadId.has_value();
    }
};