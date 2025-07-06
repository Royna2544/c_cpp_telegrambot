#include <absl/log/log.h>
#include <absl/strings/strip.h>
#include <dlfcn.h>
#include <fmt/format.h>

#include <GitBuildInfo.hpp>
#include <api/CommandModule.hpp>
#include <memory>
#include <string_view>

class DLWrapper {
    using RAIIHandle = std::unique_ptr<void, int (*)(void*)>;
    RAIIHandle handle;

    [[nodiscard]] void* sym(const std::string_view name) const {
        if (!handle) {
            throw std::invalid_argument("Handle is nullptr");
        }
        return dlsym(handle.get(), name.data());
    }

   public:
    // Constructors
    explicit DLWrapper(const std::filesystem::path& libPath)
        : handle(dlopen(libPath.string().c_str(), RTLD_NOW), &dlclose) {};
    DLWrapper() : handle(nullptr, &dlclose) {};

    // Operators
    DLWrapper& operator=(std::nullptr_t /*rhs*/) {
        handle = nullptr;
        return *this;
    }
    bool operator==(std::nullptr_t) const { return handle == nullptr; }
    operator bool() const { return handle != nullptr; }

    RAIIHandle underlying() {
        RAIIHandle tmp(nullptr, &dlclose);
        std::swap(handle, tmp);
        return tmp;
    }

    // dlfcn functions.
    template <typename T>
    [[nodiscard]] T sym(const std::string_view name) const {
        return reinterpret_cast<T>(sym(name));
    }
    bool info(const std::string_view name, Dl_info* info) const {
        // return value of 0 or more is a valid one...
        return dladdr(sym(name), info) >= 0;
    }
    static std::string_view error() {
        const char* err = dlerror();
        if (err != nullptr) {
            return err;
        }
        return "unknown";
    }
};

DynCommandModule::DynCommandModule(std::filesystem::path filePath)
    : handle(nullptr, &dlclose), filePath(std::move(filePath)) {}

bool DynCommandModule::load() {
    if (handle != nullptr) {
        LOG(WARNING) << "Preventing double loading";
        return false;
    }
    const std::string cmdNameStr =
        filePath.filename().replace_extension().string();
    absl::string_view cmdNameView(cmdNameStr);
    constexpr absl::string_view prefixView(prefix.data());

    if (!absl::ConsumePrefix(&cmdNameView, prefixView)) {
        LOG(WARNING) << "Failed to extract command name from " << filePath;
        return false;
    }

    DLWrapper dlwrapper(filePath);
    if (dlwrapper == nullptr) {
        LOG(WARNING) << fmt::format("dlopen failed for {}: {}",
                                    filePath.filename().string(),
                                    DLWrapper::error());
        return false;
    }
    DynModule* _module = dlwrapper.sym<decltype(_module)>(DYN_COMMAND_SYM_STR);
    if (_module == nullptr) {
        LOG(WARNING) << fmt::format("Failed to lookup symbol '{}' in {}",
                                    DYN_COMMAND_SYM_STR, filePath.string());
        return false;
    }

    if (_module->name == nullptr || _module->function == nullptr ||
        _module->description == nullptr) {
        LOG(ERROR) << "Invalid module: " << filePath;
        return false;
    }
    info = Info(_module);

    if constexpr (buildinfo::isDebugBuild()) {
        Dl_info dlinfo{};
        void* modulePtr = nullptr;
        if (dlwrapper.info(DYN_COMMAND_SYM_STR, &dlinfo)) {
            modulePtr = dlinfo.dli_saddr;
        } else {
            LOG(WARNING) << "dladdr failed for " << filePath << ": "
                         << DLWrapper::error();
        }

        DLOG(INFO) << fmt::format("Module {}: enforced: {}, name: {}, fn: {}",
                                  filePath.filename().string(),
                                  info.isEnforced(), _module->name,
                                  fmt::ptr(modulePtr));
    }
    handle = dlwrapper.underlying();
    return true;
}

bool DynCommandModule::unload() {
    if (handle) {
        handle = nullptr;
        return true;
    }
    LOG(WARNING) << "Attempted to unload unloaded module";
    return false;
}

bool DynCommandModule::isLoaded() const { return handle != nullptr; }
