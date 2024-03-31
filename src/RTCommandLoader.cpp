#include <BotAddCommand.h>
#include <Logging.h>
#include <RTCommandLoader.h>
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
        LOG(LogLevel::DEBUG, "Handle was at %p", handle_);
        dlclose(handle_);
        handle_ = nullptr;
    }
}

bool RTCommandLoader::loadOneCommand(std::filesystem::path _fname) {
    struct dynamicCommandModule* sym = nullptr;
    struct CommandModule* mod = nullptr;
    command_callback_t fn;
    Dl_info info{};
    void *handle, *fnptr = nullptr;
    const char* dlerrorBuf = nullptr;
    bool isSupported = true;
    const std::string fname = FS::appendDylibExtension(_fname).string();

    handle = dlopen(fname.c_str(), RTLD_NOW);
    if (!handle) {
        dlerrorBuf = dlerror();
        LOG(LogLevel::WARNING, "Failed to load: %s",
            dlerrorBuf ? dlerrorBuf : "unknown");
        return false;
    }
    sym = static_cast<struct dynamicCommandModule*>(
        dlsym(handle, DYN_COMMAND_SYM_STR));
    if (!sym) {
        LOG(LogLevel::WARNING,
            "Failed to lookup symbol '" DYN_COMMAND_SYM_STR "' in %s",
            fname.c_str());
        dlclose(handle);
        return false;
    }
    libs.emplace_back(std::make_shared<DynamicLibraryHolder>(handle));
    mod = &sym->mod;
    if (!sym->isSupported()) {
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
        LOG(LogLevel::WARNING, "dladdr failed for %s: %s", fname.c_str(),
            dlerrorBuf ? dlerrorBuf : "unknown");
    } else {
        fnptr = info.dli_saddr;
    }
    LOG(LogLevel::INFO, "Loaded RT command module from %s", fname.c_str());
    LOG(LogLevel::INFO,
        "Module dump: { enforced: %d, supported: %d, name: %s, fn: %p }",
        mod->isEnforced(), isSupported, mod->command.c_str(), fnptr);
    return true;
}

bool RTCommandLoader::loadCommandsFromFile(
    const std::filesystem::path filename) {
    std::string line;
    std::ifstream ifs(filename.string());
    if (ifs) {
        while (std::getline(ifs, line)) {
            loadOneCommand(FS::getPathForType(FS::PathType::MODULES_INSTALLED) / line);
        }
    } else {
        LOG(LogLevel::WARNING, "Failed to open %s", filename.string().c_str());
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