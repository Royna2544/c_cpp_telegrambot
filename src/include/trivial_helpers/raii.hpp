#pragma once

#include <functional>
#include <memory>
#include <type_traits>

template <typename Ref>
    requires(std::is_pointer_v<Ref>)
struct RAII {
    using Type = std::remove_pointer_t<Ref>;
    template <typename Ret>
    using Deleter = std::function<Ret(Ref)>;
    template <typename Ret>
    using Value = std::unique_ptr<Type, Deleter<Ret>>;

    template <typename Ret>
    [[nodiscard]] static auto create(Ref r, Deleter<Ret> deleter) {
        return Value<Ret>(r, deleter);
    }
};

template <typename Ref>
    requires(!std::is_pointer_v<Ref>)
struct RAII2 {
    using Type = Ref;
    using Value =
        std::unique_ptr<Type, std::function<void(std::add_pointer_t<Type>)>>;

    template <typename Ret>
    [[nodiscard]] static auto create(Type r, const std::function<Ret(Type)> deleter) {
        return Value(new Type(r), [deleter](Type *p) {
            deleter(*p);
            delete p;
        });
    }
};
