// Compiles string res xml to numbers

#include <absl/log/log.h>
#include <absl/strings/ascii.h>

#include <AbslLogInit.hpp>
#include <cstdlib>
#include <fstream>

#include "StringResLoader.hpp"
int main(int argc, char** argv) {
    StringResLoader manager;
    TgBot_AbslLogInit();

    if (argc != 3) {
        LOG(ERROR) << "Usage: " << argv[0]
                   << " <input_res_file> <output_hdr_file>";
        return EXIT_FAILURE;
    }
    LOG(INFO) << "Starting";
    LOG(INFO) << "Input file: " << argv[1];
    LOG(INFO) << "Output file: " << argv[2];

    if (!manager.parseFromFile(argv[1])) {
        return EXIT_FAILURE;
    }
    std::ofstream ofs(argv[2]);
    if (!ofs) {
        LOG(ERROR) << "Failed to open output file: " << argv[2];
        return EXIT_FAILURE;
    }
    ofs << "#pragma once" << std::endl << std::endl;
    ofs << "/* generated by " << argv[0] << " */" << std::endl;
    int index = 0;
    LOG(INFO) << "Total strings count: " << manager.m_strings.size();
    for (const auto& elem : manager.m_strings) {
        ofs << "#define STRINGRES_" << absl::AsciiStrToUpper(elem.first) << " "
            << index << std::endl;
        index++;
    }
    ofs << "#define STRINGRES_MAX " << index;
    ofs.close();
    LOG(INFO) << "Strings written to file: " << argv[2];
    LOG(INFO) << "Done";
    return 0;
}