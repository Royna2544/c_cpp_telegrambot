#pragma once

#include "api/TgBotApiImpl.hpp"

class TgBotApiImpl::OnUnknownCommandImpl {
   public:
    explicit OnUnknownCommandImpl(TgBotApiImpl::Ptr api);
};