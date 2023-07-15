#pragma once

#include <stdint.h>

#include <optional>
#include <vector>

using UserId = int64_t;

#define BLACKLIST_BUFFER 5

struct config_data {
    UserId owner_id;
    UserId blacklist[BLACKLIST_BUFFER];
    UserId whitelist[BLACKLIST_BUFFER];
};

struct TgBotConfig {
    TgBotConfig(const char* path);
    ~TgBotConfig();
    bool storeToFile(const struct config_data& data);
    bool loadFromFile(struct config_data* data);

   private:
    struct config_data* mapdata;
    bool initdone = false;
};
