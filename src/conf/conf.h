#pragma once

#include <stdint.h>

#include <optional>
#include <vector>

using UserId = int64_t;

#define DATABASE_LIST_BUFSIZE 5

struct config_data {
    UserId owner_id;
    UserId blacklist[DATABASE_LIST_BUFSIZE];
    UserId whitelist[DATABASE_LIST_BUFSIZE];
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
