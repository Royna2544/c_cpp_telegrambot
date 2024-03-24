#include <Database.h>
#include <iostream>

using database::DBWrapper;
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

#define BEGIN_CATEGORY(name) std::cout << '[' << name << ']' << std::endl
#define END_CATEGORY std::cout << std::endl
#define INDENT "   -> "

int main(int argc, char *const *argv) {
    DBWrapper.load();
    const auto mainDB = DBWrapper.protodb;
    const auto mediaDB = mainDB.mediatonames();

    std::cout << "Dump of database file: " << DBWrapper.getDatabasePath() << std::endl;
    std::cout << std::endl;
    BEGIN_CATEGORY("Generic");
    std::cout << "Owner ID: ";
    if (mainDB.has_ownerid()) {
        std::cout << mainDB.ownerid();
    } else {
        std::cout << "Not set";
    }
    std::cout << std::endl;
    END_CATEGORY;

    BEGIN_CATEGORY("Database");
    if (mainDB.has_whitelist()) {
        dumpList(mainDB.whitelist(), "whitelist");
    }
    if (mainDB.has_blacklist()) {
        dumpList(mainDB.blacklist(), "blacklist");
    }
    END_CATEGORY;

    if (const auto mediaDBSize = mediaDB.size(); mediaDBSize > 0) {
        BEGIN_CATEGORY("Media to titles");
        for (int i = 0; i < mediaDBSize; ++i) {
            const auto it = mediaDB.Get(i);
            std::cout << "- Entry " << i << ":" << std::endl;
            if (it.has_telegrammediaid())
                std::cout << INDENT "Media FileId: " << it.telegrammediaid() << std::endl;
            if (it.has_telegrammediauniqueid())
                std::cout << INDENT "Media FileUniqueId: " << it.telegrammediauniqueid() << std::endl;
            if (const auto namesSize = it.names_size(); namesSize > 0) {
                for (int j = 0; j < namesSize; ++j) {
                    std::cout << INDENT "Media Name " << j << ": " << it.names(j) << std::endl;
                }
            }
            if (i != mediaDBSize - 1)
                std::cout << std::endl;
        }
        END_CATEGORY;
    }
}
