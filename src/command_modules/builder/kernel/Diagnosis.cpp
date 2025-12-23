#include "Diagnosis.hpp"

#include <AbslLogCompat.hpp>

#include <regex>

Diagnosis::Diagnosis(const std::string_view line) {
    std::smatch match;
    std::regex pattern(

        R"(([\/\w\.-]+):(\d+):(\d+):\s+(warning|error|note):\s+([\w ;‘’?|'\[\]\"()\-.:]+)\s(\[(-[\w-]+)(=)?\])?)");

    std::string _line(line);
    if (std::regex_search(_line, match, pattern)) {
        file_path = match[1].str();
        line_number = std::stoi(match[2].str());
        column_number = std::stoi(match[3].str());
        std::string type = match[4].str();
        if (type == "warning") {
            message_type = Type::Warning;
        } else if (type == "error") {
            message_type = Type::Error;
        } else if (type == "note") {
            message_type = Type::Note;
        } else {
            LOG(ERROR) << "Invalid message type: " << type;
            message_type = Type::Error;
        }
        message = match[5].str();
        warning_code = match[7].str();
        valid = true;
        if (warning_code.empty()) {
            if (line.find("-W") != std::string::npos) {
                LOG(WARNING) << "Cannot match warning message: " << line;
                warning_code = "unknown";
                valid = false;
            }
        }
    }
}

UndefinedSym::UndefinedSym(const std::string_view line) {
    std::smatch match;
    std::regex gnu_binutils_regex(R"(undefined reference to `(\w+)')");
    std::regex clang_regex(R"(ld\.lld: error: undefined symbol: ([\w ()*]+))");
    std::string _line(line);

    if (std::regex_search(_line, match, gnu_binutils_regex)) {
        symbol = match[1].str();
        valid = true;
    } else if (std::regex_search(_line, match, clang_regex)) {
        symbol = match[1].str();
        valid = true;
    } else {
        symbol = "unknown";
    }
}