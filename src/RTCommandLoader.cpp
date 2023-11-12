#include <BotAddCommand.h>
#include <dlfcn.h>
#include <utils/libutils.h>

#include <vector>

#include "cmd_dynamic/cmd_dynamic.h"

struct DynamicLibraryHolder {
    DynamicLibraryHolder(void* handle) : handle_(handle){};
    DynamicLibraryHolder(DynamicLibraryHolder&& other) {
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }
    ~DynamicLibraryHolder() {
        if (handle_) {
            LOG_D("%s: handle was at %p", __func__, handle_);
            dlclose(handle_);
            handle_ = nullptr;
        }
    }

   private:
    void* handle_;
};

static std::vector<DynamicLibraryHolder> libs;

static void commandStub(const Bot& bot, const Message::Ptr& message) {
    bot_sendReplyMessage(bot, message, "Unsupported command");
}

void loadOneCommand(Bot& bot, const std::string& fname) {
    void* handle = dlopen(fname.c_str(), RTLD_NOW);
    if (!handle) {
        LOG_W("Failed to load: %s", dlerror() ?: "unknown");
        return;
    }
    auto* sym = static_cast<struct dynamicCommand*>(dlsym(handle, DYN_COMMAND_SYM_STR));
    if (!sym) {
        LOG_W("Failed to lookup symbol '" DYN_COMMAND_SYM_STR "' in %s", fname.c_str());
        dlclose(handle);
        return;
    }
    libs.emplace_back(handle);
    auto fn = sym->fn;
    if (!sym->isSupported()) {
        LOG_I("Module declares it is not supported.");
        fn = commandStub;
    }
    libs.emplace_back(handle);
    if (sym->enforced)
        bot_AddCommandEnforced(bot, sym->name, fn);
    else
        bot_AddCommandPermissive(bot, sym->name, fn);
    LOG_I("Loaded RT command module from %s", fname.c_str());
    LOG_I("Module dump: { enforced: %d, name: %s, fn: %p }", sym->enforced, sym->name,
          (void*)*(long*)(char*)&sym->fn);
}

void loadCommandsFromFile(Bot& bot, const std::string& filename) {
    std::string data, line;
    ReadFileToString(filename, &data);
    std::stringstream ss(data);
    while (std::getline(ss, line)) {
        static const std::string kModulesDir = "modules/";
        loadOneCommand(bot, kModulesDir + line);
    }
}
