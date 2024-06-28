#include <absl/log/log.h>

#include <AbslLogInit.hpp>
#include <cstdlib>
#include <iostream>

#include "database/bot/TgBotDatabaseImpl.hpp"

int main(const int argc, const char **argv) {
    TgBot_AbslLogInit();

    auto dbImpl = TgBotDatabaseImpl::getInstance();
    if (!dbImpl->loadDBFromConfig()) {
        LOG(ERROR) << "Failed to load database";
        return EXIT_FAILURE;
    }
    dbImpl->dump(std::cout);
    dbImpl->unloadDatabase();
}
