#include "CommandModule.h"

#include <Database.h>
#include <NamespaceImport.h>

using database::blacklist;
using database::ProtoDatabase;
using database::whitelist;

struct CommandModule cmd_addblacklist {
    .enforced = true,
    .name = "addblacklist",
    .fn = std::bind(&ProtoDatabase::addToDatabase, blacklist, pholder1, pholder2)
};

struct CommandModule cmd_rmblacklist {
    .enforced = true,
    .name = "rmblacklist",
    .fn = std::bind(&ProtoDatabase::removeFromDatabase, blacklist, pholder1, pholder2)
};

struct CommandModule cmd_addwhitelist {
    .enforced = true,
    .name = "addwhitelist",
    .fn = std::bind(&ProtoDatabase::addToDatabase, whitelist, pholder1, pholder2)
};

struct CommandModule cmd_rmwhitelist {
    .enforced = true,
    .name = "rmwhitelist",
    .fn = std::bind(&ProtoDatabase::removeFromDatabase, whitelist, pholder1, pholder2)
};