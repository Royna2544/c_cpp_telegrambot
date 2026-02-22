#include <cstdlib>
#include <filesystem>
#include "ConfigParsers.hpp"

int app_main(int argc, char** argv) {
    std::filesystem::path directory;
    if (argc != 2) {
        LOG(ERROR) << "Please provide a directory path";
        return EXIT_FAILURE;
    }
    directory = argv[1];
    if (!std::filesystem::exists(directory) ||
        !std::filesystem::is_directory(directory)) {
        LOG(ERROR) << "Invalid directory path";
        return EXIT_FAILURE;
    }
    try {
        ConfigParser parse(directory);

    } catch (const std::exception& e) {
        LOG(ERROR) << "Error parsing configuration files: " << e.what();
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
