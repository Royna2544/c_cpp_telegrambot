#pragma once

#include "api/TgBotApiImpl.hpp"

class TgBotApiImpl::RestartCommand {
    TgBotApiImpl::Ptr _api;

   public:
    explicit RestartCommand(TgBotApiImpl::Ptr api);

    void commandFunction(api::types::ParsedMessage message);
};