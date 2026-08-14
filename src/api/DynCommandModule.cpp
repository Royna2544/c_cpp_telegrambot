#include <absl/log/log.h>
#include <absl/strings/strip.h>
#include <dlfcn.h>
#include <fmt/format.h>

#include <GitBuildInfo.hpp>
#include <api/CommandModule.hpp>
#include <atomic>
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
    return loadImage(filePath);
}

bool DynCommandModule::loadImage(const std::filesystem::path& imagePath) {
    if (!mLock.try_lock()) {
        // Already concurrent.
        return false;
    }
    std::unique_lock<std::mutex> mLK(mLock, std::adopt_lock);

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

    DLWrapper dlwrapper(imagePath);
    if (dlwrapper == nullptr) {
        LOG(WARNING) << fmt::format("dlopen failed for {}: {}",
                                    imagePath.filename().string(),
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
                                  info.isPrivileged(), _module->name,
                                  fmt::ptr(modulePtr));
    }
    handle = dlwrapper.underlying();
    if (imagePath != filePath)
        loadedImagePath = imagePath;
    enableExecutions();
    return true;
}

std::unique_ptr<DynCommandModule> DynCommandModule::makeReloadCandidate()
    const {
    static std::atomic<std::uint64_t> sequence{1};
    const auto id = sequence.fetch_add(1, std::memory_order_relaxed);
    const auto imagePath =
        filePath.parent_path() / fmt::format(".glider-reload-{}-{}{}",
                                             filePath.stem().string(), id,
                                             filePath.extension().string());
    std::error_code ec;
    if (!std::filesystem::copy_file(filePath, imagePath,
                                    std::filesystem::copy_options::none, ec)) {
        LOG(ERROR) << "Cannot stage reload image " << imagePath << ": "
                   << ec.message();
        return nullptr;
    }
    auto candidate = std::make_unique<DynCommandModule>(filePath);
    if (!candidate->loadImage(imagePath)) {
        std::filesystem::remove(imagePath, ec);
        return nullptr;
    }
    return candidate;
}

void DynCommandModule::removeReloadImage() {
    if (loadedImagePath.empty())
        return;
    std::error_code ec;
    std::filesystem::remove(loadedImagePath, ec);
    if (ec) {
        LOG(WARNING) << "Cannot remove staged reload image " << loadedImagePath
                     << ": " << ec.message();
    }
    loadedImagePath.clear();
}

bool DynCommandModule::unload() {
    stopExecutions();
    auto executionLease = acquireUnloadLease();
    if (!mLock.try_lock()) {
        // Already concurrent.
        return false;
    }
    std::unique_lock<std::mutex> mLK(mLock, std::adopt_lock);

    if (handle) {
        handle = nullptr;
        removeReloadImage();
        return true;
    }
    LOG(WARNING) << "Attempted to unload unloaded module";
    return false;
}

DynCommandModule::~DynCommandModule() {
    if (handle) {
        (void)unload();
    } else {
        removeReloadImage();
    }
}

bool DynCommandModule::isLoaded() const {
    std::unique_lock<std::mutex> mLK(mLock);
    return handle != nullptr;
}
