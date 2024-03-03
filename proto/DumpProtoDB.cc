#include <Database.h>
#include <ConfigManager.h>

#include <iostream>

using database::db;
using tgbot::proto::PersonList;

static void dumpList(const PersonList& list, const char* name) {
    const int id_size = list.id_size();
    std::cout << "Dump of " << name << std::endl;
    std::cout << "Size: " << id_size << std::endl;
    if (id_size > 0) {
        for (int i = 0; i < id_size; i++)
            std::cout << "- " << list.id(i) << std::endl;
        std::cout << std::endl;
    }
}

int main(int argc, const char **argv) {
    copyCommandLine(CommandLineOp::INSERT, &argc, &argv);
    db.load();
    const auto mainDB = db.getMainDatabase();

    std::cout << "Owner ID: ";
    if (mainDB->has_ownerid()) {
        std::cout << mainDB->ownerid();
    } else {
        std::cout << "Not set";
    }
    std::cout << std::endl;
    if (mainDB->has_whitelist()) {
        dumpList(mainDB->whitelist(), "whitelist");
    }
    if (mainDB->has_blacklist()) {
        dumpList(mainDB->blacklist(), "blacklist");
    }
}
