#include <absl/log/log.h>
#include <absl/strings/strip.h>
#include <dlfcn.h>
#include <fmt/format.h>

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
        : handle(dlopen(libPath.string().c_str(), RTLD_LAZY), &dlclose){};
    DLWrapper() : handle(nullptr, &dlclose){};

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
        return std::move(tmp);
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

CommandModule::CommandModule(std::filesystem::path filePath)
    : filePath(std::move(filePath)), handle(nullptr, &dlclose) {}

bool CommandModule::load() {
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
    _module = dlwrapper.sym<decltype(_module)>(DYN_COMMAND_SYM_STR);
    if (_module == nullptr) {
        LOG(WARNING) << fmt::format("Failed to lookup symbol '{}' in {}",
                                    DYN_COMMAND_SYM_STR, filePath.string());
        return false;
    }

#ifndef NDEBUG
    Dl_info info{};
    void* modulePtr = nullptr;
    if (dlwrapper.info(DYN_COMMAND_SYM_STR, &info)) {
        modulePtr = info.dli_saddr;
    } else {
        LOG(WARNING) << "dladdr failed for " << filePath << ": "
                     << DLWrapper::error();
    }

    DLOG(INFO) << fmt::format("Module {}: enforced: {}, name: {}, fn: {}",
                              filePath.filename().string(),
                              _module->isEnforced(), _module->name,
                              fmt::ptr(modulePtr));
#endif
    handle = std::move(dlwrapper.underlying());
    return true;
}

bool CommandModule::unload() {
    if (handle) {
        handle = nullptr;
        return true;
    }
    LOG(WARNING) << "Attempted to unload unloaded module";
    return false;
}

bool DynModule::isEnforced() const { return flags & Flags::Enforced; }
bool DynModule::isHideDescription() const {
    return flags & Flags::HideDescription;
}
bool CommandModule::isLoaded() const { return handle != nullptr; }