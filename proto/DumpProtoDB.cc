#include <DatabaseBot.hpp>
#include <cstdlib>
#include <iostream>

int main(const int argc, const char **argv) {
    auto DBWrapper = DefaultDatabase();
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <databasefile>" << std::endl;
        return EXIT_FAILURE;
    }
    if (!DBWrapper.loadDatabaseFromFile(argv[1])) {
        std::cerr << "Failed to load database" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << DBWrapper << std::endl;
}
