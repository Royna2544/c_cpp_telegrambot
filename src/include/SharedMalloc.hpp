#pragma once

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <type_traits>

#include "internal/_class_helper_macros.h"

struct SharedMallocParent;

struct SharedMallocChild {
    template <typename T>
    explicit operator const T *() const {
        return static_cast<T *>(m_data);
    }
    [[nodiscard]] void *get() const { return m_data; }

    explicit SharedMallocChild(std::shared_ptr<SharedMallocParent> parent);

   private:
    std::shared_ptr<SharedMallocParent> parent;
    void *m_data{};
};

struct SharedMallocParent {
    explicit SharedMallocParent(size_t size)
        : data(malloc(size), free), size(size) {
        if (data == nullptr) {
            throw std::bad_alloc();
        }
    }

    NO_MOVE_CTOR(SharedMallocParent);
    NO_COPY_CTOR(SharedMallocParent);
    friend struct SharedMallocChild;

    // alloc allocates a new shared memory block of the specified size
    void alloc();
    size_t size{};
   private:
    std::unique_ptr<void, decltype(&free)> data;
};

struct SharedMalloc {
    explicit SharedMalloc(size_t size) {
        parent = std::make_shared<SharedMallocParent>(size);
    }
    template <typename T>
    requires (!std::is_pointer_v<T> && !std::is_integral_v<T>)
    explicit SharedMalloc(T value) {
        parent = std::make_shared<SharedMallocParent>(sizeof(T));
        memcpy(get(), &value, sizeof(T));
    }
    NO_DEFAULT_CTOR(SharedMalloc);
    SharedMallocParent *operator->() { return parent.get(); }
    bool operator!=(std::nullptr_t value) { return parent.get() != value; }

    // get returns a shared pointer to the shared memory block
    SharedMallocChild getChild() noexcept;
    [[nodiscard]] void *get() noexcept { return getChild().get(); }
    [[nodiscard]] long use_count() const noexcept { return parent.use_count(); }
    [[nodiscard]] size_t size() const noexcept { return parent->size; }

   private:
    std::shared_ptr<SharedMallocParent> parent;
};