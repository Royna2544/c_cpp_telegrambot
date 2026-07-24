#pragma once

#include <istream>
#include <string>
#include <string_view>
#include <vector>

namespace csv {

inline std::string escapeField(std::string_view field) {
    const bool quote = field.empty() ||
                       field.find_first_of(",\"\r\n") != std::string_view::npos;
    if (!quote) {
        return std::string(field);
    }
    std::string result;
    result.reserve(field.size() + 2);
    result.push_back('"');
    for (const char c : field) {
        if (c == '"') {
            result.push_back('"');
        }
        result.push_back(c);
    }
    result.push_back('"');
    return result;
}

inline std::string encodeRow(const std::vector<std::string>& fields) {
    std::string result;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i != 0) {
            result.push_back(',');
        }
        result += escapeField(fields[i]);
    }
    result.push_back('\n');
    return result;
}

inline bool tryParseRow(std::string_view row, std::vector<std::string>* output) {
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    bool afterQuote = false;

    for (std::size_t i = 0; i < row.size(); ++i) {
        const char c = row[i];
        if (quoted) {
            if (c == '"') {
                if (i + 1 < row.size() && row[i + 1] == '"') {
                    field.push_back('"');
                    ++i;
                } else {
                    quoted = false;
                    afterQuote = true;
                }
            } else {
                field.push_back(c);
            }
            continue;
        }
        if (afterQuote) {
            if (c == ',') {
                fields.push_back(std::move(field));
                field.clear();
                afterQuote = false;
            } else if (c == '\r' || c == '\n') {
                while (i + 1 < row.size() &&
                       (row[i + 1] == '\r' || row[i + 1] == '\n')) {
                    ++i;
                }
                if (i + 1 != row.size()) {
                    return false;
                }
            } else {
                return false;
            }
            continue;
        }
        if (c == '"') {
            if (!field.empty()) {
                return false;
            }
            quoted = true;
        } else if (c == ',') {
            fields.push_back(std::move(field));
            field.clear();
        } else if (c == '\r' || c == '\n') {
            while (i + 1 < row.size() &&
                   (row[i + 1] == '\r' || row[i + 1] == '\n')) {
                ++i;
            }
            if (i + 1 != row.size()) {
                return false;
            }
        } else {
            field.push_back(c);
        }
    }
    if (quoted) {
        return false;
    }
    fields.push_back(std::move(field));
    if (output != nullptr) {
        *output = std::move(fields);
    }
    return true;
}

inline std::vector<std::string> parseRow(std::string_view row) {
    std::vector<std::string> result;
    if (!tryParseRow(row, &result)) {
        return {};
    }
    return result;
}

inline bool readRow(std::istream& input, std::vector<std::string>* output) {
    std::string record;
    bool quoted = false;
    char c = 0;
    while (input.get(c)) {
        record.push_back(c);
        if (c == '"') {
            if (quoted && input.peek() == '"') {
                input.get(c);
                record.push_back(c);
            } else {
                quoted = !quoted;
            }
        } else if (c == '\n' && !quoted) {
            return tryParseRow(record, output);
        }
    }
    return !record.empty() && !quoted && tryParseRow(record, output);
}

}  // namespace csv
