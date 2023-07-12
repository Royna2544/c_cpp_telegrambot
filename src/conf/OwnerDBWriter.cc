#include <stdio.h>
#include <unistd.h>

#include <iostream>

#include "conf.h"

int main(int argc, const char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s [path to DB] [Owner User ID]\n", argv[0]);
        return 1;
    }
    bool existed = access(argv[1], R_OK | W_OK) == 0;
    TgBotConfig config(argv[1]);
    struct config_data data;
    if (existed) config.loadFromFile(&data);
    try {
        data.owner_id = std::stoi(argv[2]);
    } catch (const std::exception& e) {
        fprintf(stderr, "Error: %s failed for '%s'\n", e.what(), argv[2]);
        return 1;
    }
    config.storeToFile(data);
}
