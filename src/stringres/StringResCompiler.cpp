// Compiles string res xml to numbers

#include <absl/log/log.h>
#include <absl/strings/ascii.h>

#include <AbslLogInit.hpp>
#include <cstdlib>
#include <fstream>

#include "StringResLoader.hpp"

int main(int argc, char** argv) {
    TgBot_AbslLogInit();

    if (argc != 2) {
        LOG(ERROR) << "Usage: " << argv[0] << " <input_res_dir>";
        return EXIT_FAILURE;
    }

    StringResLoader manager(argv[1]);
    return 0;
}