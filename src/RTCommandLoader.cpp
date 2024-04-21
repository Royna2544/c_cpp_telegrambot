#include <BotAddCommand.h>
#include <RTCommandLoader.h>
#include <absl/log/log.h>
#include <dlfcn.h>

#include <filesystem>
#include <libos/libfs.hpp>
#include <memory>
#include <vector>

#include "InstanceClassBase.hpp"
#include "command_modules/runtime/cmd_dynamic.h"

DynamicLibraryHolder::DynamicLibraryHolder(DynamicLibraryHolder&& other) {
    handle_ = other.handle_;
    other.handle_ = nullptr;
}

DynamicLibraryHolder::~DynamicLibraryHolder() {
    if (handle_) {
        DLOG(INFO) << "Handle was at " << handle_;
        dlclose(handle_);
        handle_ = nullptr;
    }
}

bool RTCommandLoader::loadOneCommand(std::filesystem::path _fname) {
    struct dynamicCommandModule* sym = nullptr;
    struct CommandModule* mod = nullptr;
    command_callback_t fn;
    Dl_info info{};
    void *handle = nullptr, *fnptr = nullptr;
    const char* dlerrorBuf = nullptr;
    bool isSupported = true;
    const std::string fname = FS::appendDylibExtension(_fname).string();

    handle = dlopen(fname.c_str(), RTLD_NOW);
    if (!handle) {
        dlerrorBuf = dlerror();
        LOG(WARNING) << "Failed to load: "
                     << (dlerrorBuf ? dlerrorBuf : "unknown");
        return false;
    }
    sym = static_cast<struct dynamicCommandModule*>(
        dlsym(handle, DYN_COMMAND_SYM_STR));
    if (!sym) {
        LOG(WARNING) << "Failed to lookup symbol '" DYN_COMMAND_SYM_STR "' in "
                     << fname;
        dlclose(handle);
        return false;
    }
    libs.emplace_back(std::make_shared<DynamicLibraryHolder>(handle));
    mod = &sym->mod;
    if (sym->isSupported && !sym->isSupported()) {
        fn = commandStub;
        isSupported = false;
    } else {
        fn = mod->fn;
    }
    if (mod->isEnforced())
        bot_AddCommandEnforced(bot, mod->command, fn);
    else
        bot_AddCommandPermissive(bot, mod->command, fn);

    if (dladdr(sym, &info) < 0) {
        dlerrorBuf = dlerror();
        LOG(WARNING) << "dladdr failed for " << fname << ": "
                     << (dlerrorBuf ? dlerrorBuf : "unknown");
    } else {
        fnptr = info.dli_saddr;
    }
    LOG(INFO) << "Loaded RT command module from " << fname;
    LOG(INFO) << "Module dump: { enforced: " << mod->isEnforced()
              << ", supported: " << isSupported << ", name: " << mod->command
              << ", fn: " << fnptr << " }";
    return true;
}

bool RTCommandLoader::loadCommandsFromFile(
    const std::filesystem::path filename) {
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