#pragma once

#include <absl/log/check.h>

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <type_traits>

#include "internal/_class_helper_macros.h"

#ifndef __cpp_concepts
#define requires(x)
#endif

struct SharedMallocParent {
    explicit SharedMallocParent(size_t size)
        : data(malloc(size), free), size(size) {
        if (data == nullptr) {
            throw std::bad_alloc();
        }
    }
    explicit SharedMallocParent() : data(nullptr, free) {}

    NO_MOVE_CTOR(SharedMallocParent);
    NO_COPY_CTOR(SharedMallocParent);
    friend struct SharedMallocChild;

    // alloc allocates a new shared memory block of the specified size
    void realloc(size_t newSize) {
        if (data == nullptr) {
            data.reset(::malloc(newSize));
        } else {
            data.reset(::realloc(data.release(), newSize));
        }
        if (data == nullptr) {
            throw std::bad_alloc();
        }
        size = newSize;
    }
    [[nodiscard]] size_t getSize() const { return size; }

   private:
    size_t size{};
    std::unique_ptr<void, decltype(&free)> data;
};

struct SharedMallocChild {
    template <typename T>
    explicit operator const T *() const {
        return static_cast<T *>(m_data);
    }
    [[nodiscard]] void *get() const { return m_data; }

    explicit SharedMallocChild(std::shared_ptr<SharedMallocParent> _parent)
        : parent(std::move(_parent)), m_data(parent->data.get()) {}

   private:
    std::shared_ptr<SharedMallocParent> parent;
    void *m_data{};
};

struct SharedMalloc {
    explicit SharedMalloc(size_t size) {
        if (size == 0) {
            parent = std::make_shared<SharedMallocParent>();
        } else {
            parent = std::make_shared<SharedMallocParent>(size);
        }
    }
    SharedMalloc() { parent = std::make_shared<SharedMallocParent>(); }

    template <typename T>
        requires(!std::is_pointer_v<T> && !std::is_integral_v<T>)
    explicit SharedMalloc(T value) {
        parent = std::make_shared<SharedMallocParent>(sizeof(T));
        memcpy(get(), &value, sizeof(T));
    }

    SharedMallocParent *operator->() { return parent.get(); }
    bool operator!=(std::nullptr_t value) { return parent.get() != value; }

    template <typename T>
    explicit operator T() const {
        T value;
        assignTo(value);
        return value;
    }

    template <typename T>
    void assignTo(T *ref, size_t size, size_t offset) const {
        CHECK(size + offset <= parent->getSize())
            << ": Requested size is bigger than what's stored in memory ("
            << size + offset << " > " << parent->getSize() << ")";
        memcpy(ref, static_cast<char *>(get()) + offset, size);
    }

    template <typename T>
    void assignTo(T *ref, size_t size) const {
        assignTo(ref, size, 0);
    }

    template <typename T>
        requires(!std::is_pointer_v<T>)
    void assignTo(T &ref) const {
        assignTo(&ref, sizeof(T));
    }

    template <typename T>
        requires(std::is_class_v<T>)
    void assignFrom(T &ref) {
        CHECK(sizeof(T) == parent->getSize())
            << "Must have same size to assign from";
        assignFrom(&ref, sizeof(T));
    }

    template <typename T>
    void assignFrom(const T *ref, size_t size) {
        CHECK(size <= parent->getSize())
            << ": Requested size is bigger than what's stored in memory ("
            << size << " > " << parent->getSize() << ")";
        memcpy(get(), ref, size);
    }

    // get returns a shared pointer to the shared memory block
    [[nodiscard]] SharedMallocChild getChild() const noexcept {
        return SharedMallocChild(parent);
    }
    [[nodiscard]] void *get() const noexcept { return getChild().get(); }
    [[nodiscard]] long use_count() const noexcept { return parent.use_count(); }

    bool operator==(const SharedMalloc &other) const noexcept {
        if (parent->getSize() != other.parent->getSize()) {
            return false;
        }
        return memcmp(get(), other.get(), parent->getSize()) == 0;
    }

   private:
    std::shared_ptr<SharedMallocParent> parent;
};