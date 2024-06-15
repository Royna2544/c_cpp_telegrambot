#include "DatabaseLoader.h"

#include <absl/log/log.h>
#include <gtest/gtest.h>

#include <memory>
#include <mutex>

#include "database/bot/TgBotDatabaseImpl.hpp"

std::shared_ptr<TgBotDatabaseImpl> loadDb() {
    static std::once_flag once;
    std::call_once(once, [] {
        TgBotDatabaseImpl::getInstance()->loadDBFromConfig();
    });
    return TgBotDatabaseImpl::getInstance();
}