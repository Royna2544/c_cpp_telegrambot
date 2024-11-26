#pragma once

#include <filesystem>
#include <string>

struct Diagnosis {
    std::filesystem::path file_path;
    int line_number;
    int column_number;
    enum class Type { Note, Warning, Error } message_type;
    std::string message;
    std::string warning_code;
    bool valid = false;

    explicit Diagnosis(const std::string_view line);
};

struct UndefinedSym {
    std::string symbol;
    bool valid = false;

    explicit UndefinedSym(const std::string_view line);
};