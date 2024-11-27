#pragma once

#include <array>
#include <initializer_list>
#include <utility>
#include <vector>

#include "ConfigParsers2.hpp"

struct ToolchainConfig {
    using KeyValuePair = std::pair<std::string_view, std::string_view>;

    static std::vector<KeyValuePair> getVariables(
        KernelConfig::ClangSupport support) {
        switch (support) {
            case KernelConfig::ClangSupport::None:
                return {};
            case KernelConfig::ClangSupport::Clang:
                return {{"CC", "clang"}};
            case KernelConfig::ClangSupport::FullLLVM:
                return {
                    {"CC", "clang"},
                    {"LD", "ld.lld"},
                    {"AR", "llvm-ar"},
                    {"NM", "llvm-nm"},
                    {"OBJCOPY", "llvm-objcopy"},
                    {"OBJDUMP", "llvm-objdump"},
                };
            case KernelConfig::ClangSupport::FullLLVMWithIAS:
                return {{"LLVM", "1"}};
                // TODO: LLVM_IAS?
            default: [[unlikely]]
                return {};
        }
    }
};