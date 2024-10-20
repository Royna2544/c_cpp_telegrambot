#pragma once

#include <TgBotPPImpl_shared_depsExports.h>
#include <absl/log/log.h>

#include <StacktracePrint.hpp>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <type_traits>

#define DECLARE_CLASS_INST(type)                    \
    template <>                                     \
    std::unique_ptr<type> TGBOTPP_HELPER_DLL_EXPORT \
        InstanceClassBase<type>::instance = {}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-var-template"

template <typename T>
/**
 * @brief A base class for creating a single instance of a class
 * @tparam T the type of the instance
 */
struct InstanceClassBase {
    using pointer_type = std::add_pointer_t<T>;
    using const_pointer_type = std::add_pointer_t<std::add_const_t<T>>;

    template <typename... Args>
    static pointer_type initInstance(Args&&... args) {
        if (instance) {
            throw std::runtime_error("An instance of the class already exists");
        }
        instance = std::make_unique<T>(std::move(args)...);
        return instance.get();
    }

    /**
     * @brief Get the single instance of the class
     * @return a pointer to the instance
     */
    static pointer_type getInstance() {
        if (!instance) {
            if constexpr (std::is_default_constructible_v<T>) {
                initInstance();
            } else {
                LOG(ERROR) << "Default constructor is not available for "
                           << typeid(T).name();
                PrintStackTrace([](const std::string_view& entry) {
                    LOG(ERROR) << entry;
                    return true;
                });
                throw std::runtime_error(
                    "I need arguments for the instance, call #initInstance "
                    "first");
            }
        }
        return instance.get();
    }

    static void destroyInstance() {
        instance = nullptr;
    }
    static std::unique_ptr<T> instance;
};

#pragma clang diagnostic pop
