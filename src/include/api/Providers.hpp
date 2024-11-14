#pragma once

#include <ResourceManager.h>

#include <ManagedThreads.hpp>
#include <Random.hpp>
#include <database/DatabaseBase.hpp>
#include <trivial_helpers/fruit_inject.hpp>

#include "utils/CommandLine.hpp"
#include "utils/ConfigManager.hpp"

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

    APPLE_INJECT(Providers(Random *random, ResourceProvider *resource,
                           DatabaseBase *database, CommandLine *cmd,
                           ConfigManager *configManager,
                           ThreadManager *thread)) {
        this->random.instance = random;
        this->resource.instance = resource;
        this->database.instance = database;
        this->cmdline.instance = cmd;
        this->config.instance = configManager;
        this->threads.instance = thread;
    }
};