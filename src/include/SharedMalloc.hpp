#pragma once

#include <trivial_helpers/_class_helper_macros.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <type_traits>

struct SharedMalloc {
    using offset_type = std::ptrdiff_t;
    using size_type = int;
    using data_type = std::uint8_t;

    struct Parent {
        explicit Parent(size_type size) : _size(0), _data(nullptr, free) {
            realloc(size);
        }
        explicit Parent() : _data(nullptr, free) {}

        NO_MOVE_CTOR(Parent);
        NO_COPY_CTOR(Parent);

        // alloc allocates a new shared memory block of the specified size
        void realloc(size_type newSize) {
            if (newSize == _size) {
                return;  // No-op
            }
            if (newSize <= 0) {
                _data.reset();  // Free memory and set pointer to nullptr
                _size = 0;
                return;
            }
            data_type *newMem = nullptr;
            if (_data == nullptr) {
                newMem = static_cast<data_type *>(
                    ::calloc(newSize, sizeof(data_type)));
                if (newMem == nullptr) {
                    throw std::bad_alloc();
                }
            } else {
                auto *curmem = _data.release();
                newMem = static_cast<data_type *>(::realloc(curmem, newSize));
                if (newMem == nullptr) {
                    _data.reset(curmem);
                    throw std::bad_alloc();
                }
            }
            if (newSize > _size) {
                // Zero-fill the new block
                memset(newMem + _size, 0, newSize - _size);
            }
            _data.reset(newMem);
            _size = newSize;
        }
        [[nodiscard]] size_type size() const { return _size; }
        [[nodiscard]] data_type *data() const { return _data.get(); }

       private:
        size_type _size{};
        std::unique_ptr<data_type, decltype(&free)> _data;
    };

    explicit SharedMalloc(size_type size) {
        if (size == 0) {
            parent = std::make_shared<Parent>();
        } else {
            parent = std::make_shared<Parent>(size);
        }
    }
    explicit SharedMalloc(std::nullptr_t) : SharedMalloc() {}
    SharedMalloc() { parent = std::make_shared<Parent>(); }

    template <typename T, std::enable_if_t<std::is_class_v<T>, bool> = true>
    explicit SharedMalloc(T value) {
        parent = std::make_shared<Parent>(sizeof(T));
        assignFrom(value);
    }

    explicit operator bool() const { return parent->size() != 0; }
    bool operator!=(std::nullptr_t value) { return parent.get() != value; }
    void resize(size_t newSize) const noexcept { parent->realloc(newSize); }

    // trivial accessors
    [[nodiscard]] data_type* get() const noexcept { return parent->data(); }
    [[nodiscard]] long use_count() const noexcept { return parent.use_count(); }
    [[nodiscard]] size_type size() const noexcept { return parent->size(); }

   private:
    // A fortify check.
    inline void validateBoundsForSize(const size_type newSize) const {
        if (newSize > size()) {
            throw std::out_of_range(
                "Operation size exceeds allocated memory size");
        }
    }

    inline void offsetCheck(const offset_type offset) const {
        if (offset > size()) {
            throw std::out_of_range("Offset exceeds allocated memory bounds");
        }
    }

    [[nodiscard]] inline data_type* offsetGet(const offset_type offset) const {
        return get() + offset;
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
     * memory. Default is 0. Negative values would calculate from end of file.
     * @throws std::out_of_range if the offset or size exceeds the allocated
     * memory.
     * @note This method cannot be used with const pointers. Use `assignFrom`
     * for const pointers.
     */
    template <typename T>
    void assignTo(T *ref, const size_type size, offset_type offset = 0) const {
        static_assert(!std::is_const_v<T>,
                      "Using assignTo with a const pointer, did you mean to "
                      "use assignFrom?");
        if (offset < 0) offset += this->size();
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
    void assignTo(T &ref, const size_type offset = 0) const {
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
    void assignTo(T &ref, const size_type offset = 0) const {
        assignTo(&ref, sizeof(T), offset);
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
    void assignFrom(const T &ref, const size_type offset = 0) {
        validateBoundsForSize(sizeof(T));
        assignFrom(&ref, sizeof(T), offset);
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
    void assignFrom(const T *ref, const size_type size,
                    offset_type offset = 0) {
        if (offset < 0) offset += this->size();
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
    void move(const offset_type startOffset, const offset_type destOffset,
              const size_type size) {
        offsetCheck(startOffset);
        offsetCheck(destOffset);
        validateBoundsForSize(size + destOffset);
        memmove(offsetGet(destOffset), offsetGet(startOffset), size);
    }

    bool operator==(const SharedMalloc &other) const noexcept {
        // Check for self-comparison
        if (this == &other) {
            return true;
        }
        // Check if both shared memory blocks have same size
        if (size() != other.size()) {
            return false;
        }
        // Check if either shared memory blocks are nullptr.
        // If so, then we don't need to pass it to memcmp
        if (get() == nullptr || other.get() == nullptr) {
            return get() == other.get();
        }
        return memcmp(get(), other.get(), size()) == 0;
    }

   private:
    std::shared_ptr<Parent> parent;
};
