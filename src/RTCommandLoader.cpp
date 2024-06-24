#include <BotAddCommand.h>
#include <RTCommandLoader.h>
#include <absl/log/log.h>
#include <dlfcn.h>

#include <filesystem>
#include <libos/libfs.hpp>
#include <vector>

#include "InstanceClassBase.hpp"
#include "command_modules/runtime/cmd_dynamic.h"
#include "command_modules/CommandModule.h"

DynamicLibraryHolder::DynamicLibraryHolder(
    DynamicLibraryHolder&& other) noexcept
    : handle_(other.handle_) {
    other.handle_ = nullptr;
}

DynamicLibraryHolder::~DynamicLibraryHolder() {
    if (handle_ != nullptr) {
        dlclose(handle_);
        handle_ = nullptr;
    }
}

bool RTCommandLoader::loadOneCommand(std::filesystem::path _fname) {
    command_loader_function_t functionSym;
    Dl_info info{};
    void* handle = nullptr;
    void* fnptr = nullptr;
    const char* dlerrorBuf = nullptr;
    const std::string fname = FS::appendDylibExtension(_fname).string();

    handle = dlopen(fname.c_str(), RTLD_NOW);
    if (handle == nullptr) {
        dlerrorBuf = dlerror();
        LOG(WARNING) << "Failed to load: "
                     << ((dlerrorBuf != nullptr) ? dlerrorBuf : "unknown");
        return false;
    }
    functionSym = reinterpret_cast<void (*)(CommandModule&)>(
        dlsym(handle, DYN_COMMAND_SYM_STR));
    if (!functionSym) {
        LOG(WARNING) << "Failed to lookup symbol '" DYN_COMMAND_SYM_STR "' in "
                     << fname;
        dlclose(handle);
        return false;
    }
    CommandModule mod;
    try {
        functionSym(mod);
    } catch (const std::exception& e) {
        LOG(WARNING) << "Failed to load command from " << fname
                     << ": Loader function threw an exception: "
                     << std::quoted(e.what());
        // TODO dlclose(handle);
        return false;
    }
    libs.emplace_back(handle);
    bot_AddCommand(bot, mod.command, mod.fn, mod.isEnforced());

    if (dladdr(dlsym(handle, DYN_COMMAND_SYM_STR), &info) < 0) {
        dlerrorBuf = dlerror();
        LOG(WARNING) << "dladdr failed for " << fname << ": "
                     << ((dlerrorBuf != nullptr) ? dlerrorBuf : "unknown");
    } else {
        fnptr = info.dli_saddr;
    }
    LOG(INFO) << "Loaded RT command module from " << fname;
    LOG(INFO) << "Module dump: { enforced: " << mod.isEnforced()
              << ", name: " << mod.command << ", fn: " << fnptr << " }";
    return true;
}

bool RTCommandLoader::loadCommandsFromFile(
    const std::filesystem::path& filename) {
    std::string line;
    std::ifstream ifs(filename.string());
    if (ifs) {
        while (std::getline(ifs, line)) {
            loadOneCommand(FS::getPathForType(FS::PathType::MODULES_INSTALLED) /
                           line);
        }
    } else {
        LOG(WARNING) << "Failed to open " << filename.string();
        return false;
    }
    return true;
}

void RTCommandLoader::commandStub(const Bot& bot, const Message::Ptr& message) {
    bot_sendReplyMessage(bot, message, "Unsupported command");
}

std::filesystem::path RTCommandLoader::getModulesLoadConfPath() {
    return FS::getPathForType(FS::PathType::GIT_ROOT) / "modules.load";
}

DECLARE_CLASS_INST(RTCommandLoader);