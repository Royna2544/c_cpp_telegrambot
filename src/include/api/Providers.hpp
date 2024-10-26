#pragma once

#include <ResourceManager.h>

#include <ManagedThreads.hpp>
#include <Random.hpp>
#include <database/DatabaseBase.hpp>
#include <trivial_helpers/fruit_inject.hpp>

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
    Installable<Random> random{};
    Installable<ResourceManager> resource{};
    Installable<DatabaseBase> database{};
    Installable<ThreadManager> manager{};

    APPLE_INJECT(Providers(Random *random, ResourceManager *resource,
                           DatabaseBase *database, ThreadManager *thread)) {
        this->random.instance = random;
        this->resource.instance = resource;
        this->database.instance = database;
        this->manager.instance = thread;
    }
};