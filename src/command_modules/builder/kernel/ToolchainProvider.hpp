#pragma once

#include <bitset>

#include "Compiler.hpp"

namespace toolchains {

struct Provider {
    using CompilerBitsetType =
        std::bitset<static_cast<size_t>(Compiler::Type::MAX)>;
    using ArchBitsetType =
        std::bitset<static_cast<size_t>(KernelConfig::Arch::MAX)>;
    // The type of toolchain this provider provides
    static constexpr CompilerBitsetType type = {};
    // The target architecture this provider provides
    static constexpr ArchBitsetType arch = {};

    template <Compiler::Type... types>
    constexpr static CompilerBitsetType createCompilerBitset() {
        unsigned long long set = 0;
        ((set |= 1 << static_cast<int>(types)), ...);
        return {set};
    }

    template <KernelConfig::Arch... arches>
    constexpr static ArchBitsetType createArchBitset() {
        unsigned long long set = 0;
        ((set |= 1 << static_cast<int>(arches)), ...);
        return {set};
    }

    // Download the specified toolchain to the path
    virtual bool downloadTo(const std::filesystem::path& path) = 0;

    virtual ~Provider() = default;
};

struct GCCAndroidARMProvider : public Provider {
    static constexpr CompilerBitsetType type =
        createCompilerBitset<Compiler::Type::GCCAndroid>();
    static constexpr ArchBitsetType arch =
        createArchBitset<KernelConfig::Arch::ARM>();
    static constexpr std::string_view dirname = "gcc-arm";

    bool downloadTo(const std::filesystem::path& path) override;
};

struct GCCAndroidARM64Provider : public Provider {
    static constexpr CompilerBitsetType type =
        createCompilerBitset<Compiler::Type::GCCAndroid>();
    static constexpr ArchBitsetType arch =
        createArchBitset<KernelConfig::Arch::ARM64>();
    static constexpr std::string_view dirname = "gcc-arm64";

    bool downloadTo(const std::filesystem::path& path) override;
};

struct ClangProvider : public Provider {
    static constexpr CompilerBitsetType type =
        createCompilerBitset<Compiler::Type::Clang>();
    static constexpr ArchBitsetType arch =
        createArchBitset<KernelConfig::Arch::ARM, KernelConfig::Arch::ARM64,
                         KernelConfig::Arch::X86, KernelConfig::Arch::X86_64>();
    static constexpr std::string_view dirname = "clang";

    bool downloadTo(const std::filesystem::path& path) override;
};

}  // namespace toolchains