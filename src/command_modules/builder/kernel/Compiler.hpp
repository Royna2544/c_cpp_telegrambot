#pragma once

#include <array>

#include "ConfigParsers2.hpp"

struct Compiler {
    enum class Type { GCC = 1, GCCAndroid, Clang, MAX = Clang } type;
    std::filesystem::path _path;

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

    explicit Compiler(std::filesystem::path path);
    [[nodiscard]] std::string_view getTriple(KernelConfig::Arch arch) const;
    [[nodiscard]] std::string version() const;
};