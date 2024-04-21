#pragma once

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <utility>

#include "internal/_class_helper_macros.h"

struct SharedMallocParent;

struct SharedMallocChild {
    template <typename T>
    explicit operator const T *() const {
        return static_cast<T *>(data);
    }
    explicit operator const void *() { return static_cast<const void *>(data); }
    explicit operator void *() { return static_cast<void *>(data); }
    [[nodiscard]] void *getData() const { return static_cast<void *>(data); }

    explicit SharedMallocChild(std::shared_ptr<SharedMallocParent> parent);

   private:
    std::shared_ptr<SharedMallocParent> parent;
    void *data{};
};

struct SharedMallocParent {
    explicit SharedMallocParent(size_t size) : size(size) { alloc(); }
    ~SharedMallocParent() {
        if (data != nullptr) {
            free(data);
        }
    }
    NO_MOVE_CTOR(SharedMallocParent);
    NO_COPY_CTOR(SharedMallocParent);

    // set_size sets the size of the shared memory block
    void set_size(size_t size) noexcept;

    // alloc allocates a new shared memory block of the specified size
    void alloc();
    friend struct SharedMallocChild;

   private:
    void *data{};
    size_t size{};
};

struct SharedMalloc {
    explicit SharedMalloc(size_t size) {
        parent = std::make_shared<SharedMallocParent>(size);
    }
    NO_DEFAULT_CTOR(SharedMalloc);
    SharedMallocParent *operator->() { return parent.get(); }
    bool operator!=(std::nullptr_t value) { return parent.get() != value; }

    // get returns a shared pointer to the shared memory block
    SharedMallocChild getChild() noexcept;
    [[nodiscard]] void *getData() noexcept { return getChild().getData(); }
    [[nodiscard]] long use_count() const noexcept { return parent.use_count(); }
   private:
    std::shared_ptr<SharedMallocParent> parent;
};