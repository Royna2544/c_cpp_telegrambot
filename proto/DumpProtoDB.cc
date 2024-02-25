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

int main(const int argc, const char **argv) {
    copyCommandLine(argc, argv, nullptr, nullptr);
    db.load();

    std::cout << "Owner ID: ";
    if (db->has_ownerid()) {
        std::cout << db->ownerid();
    } else {
        std::cout << "Not set";
    }
    std::cout << std::endl;
    if (db->has_whitelist()) {
        dumpList(db->whitelist(), "whitelist");
    }
    if (db->has_blacklist()) {
        dumpList(db->blacklist(), "blacklist");
    }
}
