#include <CommandLine.hpp>

CommandLine::CommandLine(CommandLine::argc_type argc,
                         CommandLine::argv_type argv)
    : _argc(argc), _argv(argv) {}

CommandLine::argv_type CommandLine::argv() const { return _argv; }

CommandLine::argc_type CommandLine::argc() const { return _argc; }

std::filesystem::path CommandLine::exe() const {
    if (_argv == nullptr || _argv[0] == nullptr) {
        return {};
    }
    return startingDirectory / _argv[0];
}

bool CommandLine::operator==(const CommandLine& other) const {
    if (_argc != other._argc) {
        return false;
    }
    for (int i = 0; i < _argc; ++i) {
        if (std::string_view(_argv[i]) != std::string_view(other._argv[i])) {
            return false;
        }
    }
    return true;
}