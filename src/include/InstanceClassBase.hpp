#pragma once

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

#define DECLARE_CLASS_INST(type) \
    template <>                  \
    std::shared_ptr<type> InstanceClassBase<type>::instance = {}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-var-template"

template <typename T>
/**
 * @brief A base class for creating a single instance of a class
 * @tparam T the type of the instance
 */
struct InstanceClassBase {
    template <typename... Args>
    static void initInstance(Args&&... args) {
        instance = std::make_shared<T>(std::forward<Args>(args)...);
    }
    /**
     * @brief Get the single instance of the class
     * @return a pointer to the instance
     */
    static std::shared_ptr<T> getInstance() {
        if (!instance) {
            if constexpr (std::is_default_constructible_v<T>) {
                initInstance();
            } else {
                throw std::runtime_error(
                    "I need arguments for the instance, call #initInstance "
                    "first");
            }
        }
        return instance;
    }
    static std::shared_ptr<T> instance;
};

#pragma clang diagnostic pop