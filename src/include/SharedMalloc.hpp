#pragma once

#include <absl/log/check.h>

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <stdexcept>
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
    inline void validateBoundsForSize(const size_t newSize) const {
        DCHECK_LE(newSize, parent->size())
            << ": Operation size exceeds allocated memory size";
        if (newSize > parent->size()) {
            throw std::out_of_range(
                "Operation size exceeds allocated memory size");
        }
    }

    inline void offsetCheck(const size_t offset) const {
        DCHECK_LE(offset, parent->size())
            << ": Offset exceeds allocated memory bounds";
        if (offset > parent->size()) {
            throw std::out_of_range("Offset exceeds allocated memory bounds");
        }
    }

    [[nodiscard]] inline char *offsetGet(const size_t offset) const {
        return static_cast<char *>(get()) + offset;
    }

   public:
    /**
     * @brief Assigns a block of memory from this object to the destination
     * pointed to by `ref`.
     *
     * Copies `size` bytes from the internal memory (starting from the given
     * `offset`) to the memory pointed to by `ref`. This method ensures that the
     * operation does not exceed memory bounds.
     *
     * @tparam T The type of the destination object pointed to by `ref`.
     * @param ref Pointer to the destination where data will be copied to.
     * @param size Number of bytes to be copied.
     * @param offset Offset from which to start copying from the internal
     * memory. Default is 0.
     * @throws std::out_of_range if the offset or size exceeds the allocated
     * memory.
     * @note This method cannot be used with const pointers. Use `assignFrom`
     * for const pointers.
     */
    template <typename T>
    void assignTo(T *ref, const size_t size, const size_t offset = 0) const {
        static_assert(!std::is_const_v<T>,
                      "Using assignTo with a const pointer, did you mean to "
                      "use assignFrom?");
        offsetCheck(offset);
        validateBoundsForSize(size + offset);
        memcpy(ref, offsetGet(offset), size);
    }

    /**
     * @brief Assigns an object from this internal memory to the reference
     * `ref`.
     *
     * Copies the entire object (of type T) from the internal memory (starting
     * from the given `offset`) to the destination object `ref`.
     *
     * @tparam T The type of the object to copy.
     * @param ref Reference to the object where data will be copied to.
     * @param offset Offset from which to start copying from the internal
     * memory.
     * @throws std::out_of_range if the offset or size exceeds the allocated
     * memory.
     */
    template <typename T>
    void assignTo(T &ref, const size_t offset) const {
        assignTo(&ref, sizeof(T), offset);
    }

    /**
     * @brief Assigns an object from this internal memory to the reference
     * `ref`.
     *
     * Copies the entire object (of type T) from the internal memory to the
     * destination object `ref`.
     *
     * @tparam T The type of the object to copy. Cannot be a pointer type.
     * @param ref Reference to the object where data will be copied to.
     * @throws std::out_of_range if the size exceeds the allocated memory.
     */
    template <typename T>
        requires(!std::is_pointer_v<T>)
    void assignTo(T &ref) const {
        assignTo(&ref, sizeof(T));
    }

    /**
     * @brief Assigns data from a source object to this internal memory.
     *
     * Copies the entire object (of type T) from `ref` into the internal memory,
     * ensuring that the internal memory size is sufficient.
     *
     * @tparam T The type of the object to assign. Cannot be a pointer type.
     * @param ref Reference to the source object.
     * @throws std::out_of_range if the size of T exceeds the allocated memory.
     */
    template <typename T>
        requires(!std::is_pointer_v<T>)
    void assignFrom(const T &ref) {
        CHECK_LE(sizeof(T), parent->size())
            << ": *this Must have bigger size than sizeof(T)";
        assignFrom(&ref, sizeof(T));
    }

    /**
     * @brief Assigns a block of memory from a source pointer `ref` to this
     * internal memory.
     *
     * Copies `size` bytes from the memory pointed to by `ref` into this
     * object's internal memory, starting from the specified `offset`.
     *
     * @tparam T The type of the source data.
     * @param ref Pointer to the source data to be copied.
     * @param size Number of bytes to be copied.
     * @param offset Offset within the internal memory where the data should be
     * copied to. Default is 0.
     * @throws std::out_of_range if the offset or size exceeds the allocated
     * memory.
     */
    template <typename T>
    void assignFrom(const T *ref, const size_t size, const size_t offset = 0) {
        offsetCheck(offset);
        validateBoundsForSize(size + offset);
        memcpy(offsetGet(offset), ref, size);
    }

    /**
     * @brief Moves a block of memory within the internal memory.
     *
     * Moves `size` bytes of memory starting from `startOffset` to the position
     * starting at `destOffset` within the internal memory. This operation
     * supports overlapping memory regions.
     *
     * @param startOffset Offset from which to start moving memory.
     * @param destOffset Offset to where the memory should be moved.
     * @param size Number of bytes to move.
     * @throws std::out_of_range if either the startOffset, destOffset, or size
     * exceeds the allocated memory.
     */
    void move(const size_t startOffset, const size_t destOffset,
              const size_t size) {
        offsetCheck(startOffset);
        offsetCheck(destOffset);
        validateBoundsForSize(size + destOffset);
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