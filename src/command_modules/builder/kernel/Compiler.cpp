#include "Compiler.hpp"

#include <absl/strings/match.h>

#include <algorithm>
#include <regex>
#include <stdexcept>

#include "ConfigParsers2.hpp"
#include "command_modules/builder/ForkAndRun.hpp"

Compiler::Compiler(std::filesystem::path toolchainPath, KernelConfig::Arch arch,
                   Type type)
    : type(type), _rootPath(std::move(toolchainPath)) {
    _path = _rootPath / "bin";

    switch (type) {
        case Type::GCC:
        case Type::GCCAndroid:
            _path /= std::string(triple(arch)).append("gcc");
            break;
        case Type::Clang:
            _path /= "clang";
            break;
        default:
            throw std::invalid_argument("Unsupported compiler type");
    }
    LOG(INFO) << "Compiler CC path: " << _path;
    compilerVersion = _version();
    LOG(INFO) << "version: " << compilerVersion;
}

std::string_view Compiler::triple(KernelConfig::Arch arch) const {
    // clang-format off
    return std::ranges::find_if(std::ranges::find_if(kCompilersTripleMap, [this](const PerCompilerTripleMapPair& map) {
        return map.first == type;
    })->second, [arch](const ArchAndValuePair& pair) {
        return pair.first == arch;
    })->second;
    // clang-format on
}

class OutputGetter : private ForkAndRun {
    std::string output;
    std::string error;
    std::filesystem::path _exe_path;

    void handleStdoutData(BufferViewType buffer) override {
        output += buffer.data();
    }
    void handleStderrData(BufferViewType buffer) override {
        error += buffer.data();
    }
    DeferredExit runFunction() override {
        return ForkAndRunSimple(fmt::format("{} --version", _exe_path.string()))
            .execute();
    }

   public:
    explicit OutputGetter(std::filesystem::path exe_path)
        : _exe_path(std::move(exe_path)) {}

    std::string get() {
        if (!execute()) {
            throw std::runtime_error("Failed to execute compiler: " +
                                     _exe_path.string());
        }
        if (!error.empty()) {
            throw std::runtime_error(error);
        }
        return output;
    }
};

std::string Compiler::_version() const {
    OutputGetter getter(_path);
    std::string version;
    try {
        version = getter.get();
    } catch (const std::runtime_error& ex) {
        LOG(ERROR) << ex.what();
        return {};
    }
    std::smatch match;
    switch (type) {
        case Type::GCC:
        case Type::GCCAndroid: {
            std::regex gcc_version_re(R"((.*?gcc \(.*\) \d+(\.\d+)*))");
            if (std::regex_search(version, match, gcc_version_re)) {
                return match[1].str();
            }
        }
        case Type::Clang: {
            std::regex clang_version_re(R"((.*?clang version \d+(\.\d+)*).*)");
            if (std::regex_search(version, match, clang_version_re)) {
                return match[1].str();
            }
        }
        default:
            [[unlikely]] return {};
    }
}

std::string Compiler::version() const {
    return compilerVersion;
}

std::filesystem::path Compiler::path() const { return _rootPath; }