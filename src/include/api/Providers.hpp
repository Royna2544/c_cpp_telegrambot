#pragma once

#include <ManagedThreads.hpp>
#include <database/DatabaseBase.hpp>
#include <random/Random.hpp>
#include <trivial_helpers/fruit_inject.hpp>

#include "AuthContext.hpp"
#include "utils/CommandLine.hpp"
#include "utils/ConfigManager.hpp"
#include "utils/ResourceManager.hpp"

// Providers to supply DI
class Providers {
    // 'Installable' subcomponents
    template <typename T>
    struct Installable {
        T *instance;

        [[nodiscard]] T *get() const { return instance; }
        T *operator->() const { return get(); }
    };

   public:
    Installable<RandomBase> random{};
    Installable<ResourceProvider> resource{};
    Installable<DatabaseBase> database{};
    Installable<CommandLine> cmdline{};
    Installable<ConfigManager> config{};
    Installable<ThreadManager> threads{};
    Installable<AuthContext> auth{};

    APPLE_INJECT(Providers(RandomBase *random, ResourceProvider *resource,
                           DatabaseBase *database, CommandLine *cmd,
                           ConfigManager *configManager, ThreadManager *thread,
                           AuthContext *auth)) {
        this->random.instance = random;
        this->resource.instance = resource;
        this->database.instance = database;
        this->cmdline.instance = cmd;
        this->config.instance = configManager;
        this->threads.instance = thread;
        this->auth.instance = auth;
    }
};
