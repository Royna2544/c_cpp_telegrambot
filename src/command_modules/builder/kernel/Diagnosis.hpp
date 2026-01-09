#pragma once

#include <filesystem>
#include <string>

class Diagnosis {
    bool valid = false;

   public:
    std::filesystem::path file_path;
    int line_number;
    int column_number;
    enum class Type { Note, Warning, Error } message_type;
    std::string message;
    std::string warning_code;

    explicit Diagnosis(const std::string_view line);
    explicit operator bool() const { return valid; }
};

class UndefinedSym {
    bool valid = false;

   public:
    std::string symbol;

    explicit UndefinedSym(const std::string_view line);
    explicit operator bool() const { return valid; }
};