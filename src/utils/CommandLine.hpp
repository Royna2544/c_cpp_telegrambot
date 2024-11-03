#pragma once

#include <TgBotUtilsExports.h>

#include <filesystem>

class TgBotUtils_API CommandLine {
   public:
    using argv_type = char* const*;
    using argc_type = int;

   private:
    argc_type _argc;
    argv_type _argv;
    std::filesystem::path startingDirectory;

   public:
    CommandLine(argc_type argc, argv_type argv);
    [[nodiscard]] argv_type argv() const;
    [[nodiscard]] argc_type argc() const;
    [[nodiscard]] std::filesystem::path exe() const;

    bool operator==(const CommandLine& other) const;
};