#include <RTCommandLoader.h>
#include <absl/log/log.h>
#include <dlfcn.h>

#include <filesystem>
#include <libos/libfs.hpp>
#include <vector>

#include "InstanceClassBase.hpp"
#include "TgBotWrapper.hpp"

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

// Sym contains const char *cmdname, CommandModule& out, return if success
bool RTCommandLoader::loadOneCommand(std::filesystem::path fname) {
    Dl_info info{};
    void* handle = nullptr;
    void* fnptr = nullptr;
    const char* dlerrorBuf = nullptr;
    bool (*sym)(const char*, CommandModule&) = nullptr;
    CommandModule module;
    std::array<char, 20> cmdName{};

    handle = dlopen(fname.string().c_str(), RTLD_NOW);
    if (handle == nullptr) {
        dlerrorBuf = dlerror();
        LOG(WARNING) << "Failed to load: "
                     << ((dlerrorBuf != nullptr) ? dlerrorBuf : "unknown");
        return false;
    }
    sym = reinterpret_cast<decltype(sym)>(dlsym(handle, DYN_COMMAND_SYM_STR));
    if (sym == nullptr) {
        LOG(WARNING) << "Failed to lookup symbol '" DYN_COMMAND_SYM_STR "' in "
                     << fname;
        dlclose(handle);
        return false;
    }
    libs.emplace_back(handle);

    if (sscanf(fname.filename().replace_extension().string().c_str(),
               "libcmd_%s", cmdName.data()) != 1) {
        LOG(WARNING) << "scanf failed for " << fname;
        dlclose(handle);
        return false;
    }

    if (!sym(cmdName.data(), module)) {
        LOG(WARNING) << "Failed to load command module from " << fname;
        module.fn = nullptr; // Prevent double free from function ptr dtor...
        dlclose(handle);
        return false;
    }

    TgBotWrapper::getInstance()->addCommand(module);

    if (dladdr(dlsym(handle, DYN_COMMAND_SYM_STR), &info) < 0) {
        dlerrorBuf = dlerror();
        LOG(WARNING) << "dladdr failed for " << fname << ": "
                     << ((dlerrorBuf != nullptr) ? dlerrorBuf : "unknown");
    } else {
        fnptr = info.dli_saddr;
    }
    //    DLOG(INFO) << "Loaded RT command module from " << fname;
    //    DLOG(INFO) << "Module dump: { enforced: " << module.isEnforced()
    //               << ", name: " << module.command << ", fn: " << fnptr << "
    //               }";
    return true;
}

void RTCommandLoader::doInitCall() {
    for (auto i = std::filesystem::directory_iterator(
             FS::getPathForType(FS::PathType::BUILD_ROOT));
         i != std::filesystem::directory_iterator(); ++i) {
        if (i->path().extension() == FS::kDylibExtension &&
            i->path().filename().string().starts_with("libcmd_")) {
            LOG(INFO) << "Loading " << i->path().filename() << "...";
            loadOneCommand(i->path());
        }
    }
}

DECLARE_CLASS_INST(RTCommandLoader);