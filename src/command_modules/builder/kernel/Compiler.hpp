#pragma once

#include <array>

#include "ConfigParsers2.hpp"

class Compiler {
   public:
    enum class Type { GCC = 1, GCCAndroid, Clang, MAX = Clang };

   private:
    Type type;
    std::filesystem::path _path;      // Path of CC
    std::filesystem::path _rootPath;  // Path of toolchain root
    std::string compilerVersion;

    using ArchAndValuePair = std::pair<KernelConfig::Arch, std::string_view>;
    using PerCompilerTripleMap =
        std::array<ArchAndValuePair, static_cast<int>(KernelConfig::Arch::MAX)>;
    using PerCompilerTripleMapPair = std::pair<Type, PerCompilerTripleMap>;

    constexpr static PerCompilerTripleMap kGCCTripleMap = {
        ArchAndValuePair{KernelConfig::Arch::ARM, "arm-linux-gnueabi-"},
        {KernelConfig::Arch::ARM64, "aarch64-linux-gnu-"},
        {KernelConfig::Arch::X86, "i686-linux-gnu-"},
        {KernelConfig::Arch::X86_64, "x86_64-linux-gnu-"}};

    constexpr static PerCompilerTripleMap kGCCAndroidTripleMap = {
        ArchAndValuePair{KernelConfig::Arch::ARM, "arm-linux-androideabi-"},
        {KernelConfig::Arch::ARM64, "aarch64-linux-android-"},
        {KernelConfig::Arch::X86, "i686-linux-android-"},
        {KernelConfig::Arch::X86_64, "x86_64-linux-android-"}};

    constexpr static std::array<PerCompilerTripleMapPair,
                                static_cast<int>(Type::MAX)>
        kCompilersTripleMap = {
            PerCompilerTripleMapPair{Type::GCCAndroid, kGCCAndroidTripleMap},
            PerCompilerTripleMapPair{Type::GCC, kGCCTripleMap},
            PerCompilerTripleMapPair{Type::Clang, kGCCTripleMap}};
            
    [[nodiscard]] std::string _version() const;

   public:
    explicit Compiler(std::filesystem::path toolchainPath,
                      KernelConfig::Arch arch, Type type);
    [[nodiscard]] std::string_view triple(KernelConfig::Arch arch) const;
    [[nodiscard]] std::string version() const;
    [[nodiscard]] std::filesystem::path path() const;
};