#include <TgBotCommandLineExports.h>

#include <string>
#include <vector>

#include "InstanceClassBase.hpp"

class TgBotCommandLine_API CommandLine : public InstanceClassBase<CommandLine> {
   public:
    explicit CommandLine(int argc, char *argv[]) {
        arguments_.reserve(argc);
        for (int i = 0; i < argc; ++i) {
            arguments_.emplace_back(argv[i]);
        }
        argv_ = argv;  // Store the argv for later use
    }

    [[nodiscard]] std::vector<std::string> getArguments() const {
        return arguments_;
    }
    std::string operator[](int i) const {
        return arguments_.at(i);
    }
    // Returns the original argv array for functions like execve
    [[nodiscard]] char **getArgv() const { return argv_; }

   private:
    std::vector<std::string> arguments_;
    // Not owned by CommandLine, it is valid throughout the
    // lifetime of the program
    char **argv_;
};