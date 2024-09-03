#include <tgbot/types/ReplyParameters.h>

#include <memory>

#include "Types.h"

// Extension of ReplyParameters
struct ReplyParametersExt : public TgBot::ReplyParameters {
    typedef std::shared_ptr<ReplyParametersExt> Ptr;
    static constexpr MessageThreadId kThreadIdNone = 0;

    MessageThreadId messageThreadId{};

    [[nodiscard]] bool hasThreadId() const {
        return messageThreadId != kThreadIdNone;
    }
};