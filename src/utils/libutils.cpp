#include "libutils.h"

#include <RuntimeException.h>
#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>

#include <boost/config.hpp>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <random>
#include <stdexcept>

#include "../popen_wdt/popen_wdt.h"

#define ARRAY_SIZE(arr) sizeof(arr) / sizeof(arr[0])

namespace fs = std::filesystem;

std::string getCompileVersion() {
    static std::string compileinfo(BOOST_PLATFORM " | " BOOST_COMPILER " | " __DATE__);
    return compileinfo;
}

bool ReadFileToString(const std::string& path, std::string* content) {
    std::error_code ec;
    std::ifstream ifs;
    std::string str;
    auto filesize = fs::file_size(path, ec);

    if (ec) {
        LOG_E("Failed to determine file size: '%s', %s", path.c_str(), ec.message().c_str());
        return false;
    }

    ifs.exceptions(std::ifstream::badbit);
    try {
        ifs.open(path);
    } catch (const std::ifstream::failure& e) {
        LOG_E("Failed to open file: '%s', %s", path.c_str(), e.what());
        return false;
    }

    content->clear();
    content->reserve(filesize);
    while (std::getline(ifs, str)) {
        *content += str + '\n';
    }
    return true;
}

std::vector<std::string> getPathEnv() {
    size_t pos = 0;
    std::vector<std::string> paths;
    std::string path;
    const char* path_c = getenv("PATH");
    if (!path_c) {
        return {};
    }
    path = path_c;
    while ((pos = path.find(path_env_delimiter)) != std::string::npos) {
        paths.emplace_back(path.substr(0, pos));
        path.erase(0, pos + 1);
    }
    return paths;
}

std::string findCommandExe(const std::string& command) {
    static const bool is_windows = IS_DEFINED(__WIN32);
    static char buffer[PATH_MAX];
    std::string _command = command;
    if (is_windows)
        _command.append(".exe");
    for (const auto& path : getPathEnv()) {
        if (path.empty()) continue;
        memset(buffer, 0, sizeof(buffer));
        size_t bytes = snprintf(buffer, sizeof(buffer), "%s%c%s",
                                path.c_str(), dir_delimiter, _command.c_str());
        if (bytes < sizeof(buffer))
            buffer[bytes] = '\0';
        if (canExecute(buffer))
            return std::string(buffer);
    }
    return {};
}

std::string findCompiler(ProgrammingLangs lang) {
    static std::map<ProgrammingLangs, std::vector<std::string>> compilers = {
        {ProgrammingLangs::C, {"clang", "gcc", "cc"}},
        {ProgrammingLangs::CXX, {"clang++", "g++", "c++"}},
        {ProgrammingLangs::GO, {"go"}},
        {ProgrammingLangs::PYTHON, {"python", "python3"}},
    };
    for (const auto& options : compilers[lang]) {
        auto ret = findCommandExe(options);
        if (!ret.empty()) return ret;
    }
    return {};
}

int genRandomNumber(const int num1, const int num2) {
    std::random_device rd;
    std::mt19937 gen(rd());
    int num1_l = num1, num2_l = num2;
    if (num1 > num2) {
        num1_l = num2;
        num2_l = num1;
    }
    std::uniform_int_distribution<int> distribution(num1_l, num2_l);
    return distribution(gen);
}

bool runCommand(const std::string& command, std::string& result) {
    auto fp = popen_watchdog(command.c_str(), nullptr);
    static char buffer[512] = {0};
    if (!fp) return false;
    while (fgets(buffer, sizeof(buffer), fp)) {
        result += buffer;
        memset(buffer, 0, sizeof(buffer));
    }
    if (result.back() == '\n')
        result.pop_back();
    return true;
}

std::string getSrcRoot() {
    static std::string dir;
    static std::once_flag flag;
    std::call_once(flag, [] {
        if (!runCommand("git rev-parse --show-toplevel", dir)) {
            throw std::runtime_error("Command failed");
        }
    });
    return dir;
}

std::string getMIMEString(const std::string& path) {
    static std::once_flag once;
    static rapidjson::Document doc;
    std::string extension = fs::path(path).extension().string();

    std::call_once(once, [] {
        std::string buf;
        ReadFileToString(getResourcePath("mimeData.json"), &buf);
        doc.Parse(buf.c_str());
        if (doc.HasParseError())
            throw runtime_errorf("Failed to parse mimedata: %d", doc.GetParseError());
    });
    if (!extension.empty()) {
        for (rapidjson::SizeType i = 0; i < doc.Size(); i++) {
            const rapidjson::Value& oneJsonElement = doc[i];
            const rapidjson::Value& availableTypes = oneJsonElement["types"].GetArray();
            for (rapidjson::SizeType i = 0; i < availableTypes.Size(); i++) {
                if (availableTypes[i].GetString() == extension) {
                    auto mime = oneJsonElement["name"].GetString();
                    LOG_D("Found MIME type: '%s'", mime);
                    return mime;
                }
            }
        }
        LOG_W("Unknown file extension: '%s'", extension.c_str());
    }
    return "application/octet-stream";
}
