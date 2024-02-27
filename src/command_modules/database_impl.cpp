#include <functional>

#include <Database.h>
#include "CommandModule.h"

using database::blacklist;
using database::ProtoDatabase;
using database::whitelist;

struct CommandModule cmd_addblacklist {
    .enforced = true,
    .name = "addblacklist",
    .fn = std::bind(&ProtoDatabase::addToDatabase, blacklist, std::placeholders::_1, std::placeholders::_2)
};

struct CommandModule cmd_rmblacklist {
    .enforced = true,
    .name = "rmblacklist",
    .fn = std::bind(&ProtoDatabase::removeFromDatabase, blacklist, std::placeholders::_1, std::placeholders::_2)
};

struct CommandModule cmd_addwhitelist {
    .enforced = true,
    .name = "addwhitelist",
    .fn = std::bind(&ProtoDatabase::addToDatabase, whitelist, std::placeholders::_1, std::placeholders::_2)
};

struct CommandModule cmd_rmwhitelist {
    .enforced = true,
    .name = "rmwhitelist",
    .fn = std::bind(&ProtoDatabase::removeFromDatabase, whitelist, std::placeholders::_1, std::placeholders::_2)
};