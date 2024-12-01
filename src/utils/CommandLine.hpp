#pragma once

#include <UtilsExports.h>

#include <filesystem>

namespace FS {

enum class PathType {
    INSTALL_ROOT,
    RESOURCES,
    RESOURCES_SQL,
    RESOURCES_WEBPAGE,
    RESOURCES_SCRIPTS,
    CMD_MODULES
};

}

class Utils_API CommandLine {
   public:
    using argv_type = char* const*;
    using argc_type = int;

   private:
    argc_type _argc;
    argv_type _argv;
    std::filesystem::path exePath;

   public:
    CommandLine(argc_type argc, argv_type argv);
    [[nodiscard]] argv_type argv() const;
    [[nodiscard]] argc_type argc() const;
    [[nodiscard]] std::filesystem::path exe() const;

    bool operator==(const CommandLine& other) const;

    [[nodiscard]] std::filesystem::path getPath(FS::PathType type) const;
};