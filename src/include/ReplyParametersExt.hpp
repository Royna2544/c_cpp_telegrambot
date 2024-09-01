#include <tgbot/types/ReplyParameters.h>
#include <memory>
#include "Types.h"

// Extension of ReplyParameters
struct ReplyParametersExt : public TgBot::ReplyParameters {
    typedef std::shared_ptr<ReplyParametersExt> Ptr;
    
    MessageThreadId messageThreadId{};
};