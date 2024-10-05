#pragma once

#include <sys/mman.h>

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
    static auto create(Ref r, Deleter<Ret> deleter) {
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
    static auto create(Type r, const std::function<Ret(Type)> deleter) {
        return Value(new Type(r), [deleter](Type *p) {
            deleter(*p);
            delete p;
        });
    }
};

/**
 * @brief Creates a unique_ptr that automatically closes the given file
 * descriptor when it goes out of scope.
 *
 * @param[in, out] fd A pointer to the file descriptor to be managed.
 *
 * @return A unique_ptr that owns the given file descriptor and will
 * automatically close it when it goes out of scope.
 */
inline auto createAutoCloser(int fd) {
    return RAII2<int>::create<int>(fd, close);
}

inline auto createAutoUnmapper(void *mem, size_t size) {
    return RAII<void *>::create<void>(mem,
                                      [size](void *mem) { munmap(mem, size); });
}