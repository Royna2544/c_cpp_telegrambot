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
        : data(malloc(size), free), _size(size) {
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
        _size = newSize;
    }
    [[nodiscard]] size_t size() const { return _size; }

   private:
    size_t _size{};
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
    explicit SharedMalloc(T value) {
        parent = std::make_shared<SharedMallocParent>(sizeof(T));
        assignFrom(value);
    }

    SharedMallocParent *operator->() const { return parent.get(); }
    bool operator!=(std::nullptr_t value) { return parent.get() != value; }

    template <typename T>
    explicit operator T() const {
        T value;
        assignTo(value);
        return value;
    }

   private:
    // A fortify check.
    inline void newSizeCheck(const size_t newSize) const {
        CHECK_LE(newSize, parent->size())
            << ": Requested size is bigger than this->size()";
    }

    inline void offsetCheck(const size_t offset) const {
        CHECK_LE(offset, parent->size())
            << ": Requested offset is bigger than this->size()";
    }

    [[nodiscard]] inline char *offsetGet(const size_t offset) const {
        return static_cast<char *>(get()) + offset;
    }

   public:
    template <typename T>
    void assignTo(T *ref, const size_t size, const size_t offset = 0) const {
        static_assert(!std::is_const_v<T>,
                      "Using assignTo to a const pointer, did you mean to use "
                      "assignFrom?");
        offsetCheck(offset);
        newSizeCheck(size + offset);
        memcpy(ref, static_cast<char *>(get()) + offset, size);
    }

    template <typename T>
        requires(!std::is_pointer_v<T>)
    void assignTo(T &ref) const {
        assignTo(&ref, sizeof(T));
    }

    template <typename T>
        requires(!std::is_pointer_v<T>)
    void assignFrom(const T &ref) {
        CHECK_LE(sizeof(T), parent->size())
            << ": *this Must have bigger size than sizeof(T)";
        assignFrom(&ref, sizeof(T));
    }

    template <typename T>
    void assignFrom(const T *ref, const size_t size, const size_t offset = 0) {
        offsetCheck(offset);
        newSizeCheck(size + offset);
        memcpy(offsetGet(offset), ref, size);
    }

    // Move memory of [startOffset] with size of [size] to [newOffset]
    void move(const size_t startOffset, const size_t destOffset,
              const size_t size) {
        offsetCheck(startOffset);
        offsetCheck(destOffset);
        newSizeCheck(size + destOffset);
        memmove(offsetGet(destOffset), offsetGet(startOffset), size);
    }

    // get returns a shared pointer to the shared memory block
    [[nodiscard]] SharedMallocChild getChild() const noexcept {
        return SharedMallocChild(parent);
    }
    [[nodiscard]] void *get() const noexcept { return getChild().get(); }
    [[nodiscard]] long use_count() const noexcept { return parent.use_count(); }

    bool operator==(const SharedMalloc &other) const noexcept {
        // Check for self-comparison
        if (this == &other) {
            return true;
        }
        // Check if both shared memory blocks have same size
        if (parent->size() != other.parent->size()) {
            return false;
        }
        // Check if either shared memory blocks are nullptr.
        // If so, then we don't need to pass it to memcmp
        if (get() == nullptr || other.get() == nullptr) {
            return get() == other.get();
        }
        return memcmp(get(), other.get(), parent->size()) == 0;
    }

   private:
    std::shared_ptr<SharedMallocParent> parent;
};