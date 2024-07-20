#include <string>
#include <vector>
#include "InstanceClassBase.hpp"
#include <TgBotCommandLineExports.h>

class TgBotCommandLine_API CommandLine : public InstanceClassBase<CommandLine> {
   public:
    explicit CommandLine(int argc, char *argv[]) {
        arguments_.reserve(argc);
        for (int i = 0; i < argc; ++i) {
            arguments_.emplace_back(argv[i]);
        }
    }

    [[nodiscard]] std::vector<std::string> getArguments() const { return arguments_; }
    std::string operator[](int i) const { return arguments_.at(i); }

   private:
    std::vector<std::string> arguments_;
};