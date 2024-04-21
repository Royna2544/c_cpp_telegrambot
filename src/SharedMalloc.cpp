#include <absl/log/log.h>

#include <SharedMalloc.hpp>

SharedMallocChild::SharedMallocChild(std::shared_ptr<SharedMallocParent> _parent)
    : parent(std::move(_parent)), data(parent->data) {}

SharedMallocChild SharedMalloc::getChild() noexcept {
    return SharedMallocChild(parent);
}

void SharedMallocParent::alloc() {
    if (data == nullptr) {
        data = malloc(size);
    } else {
        data = realloc(data, size);
    }
    if (data == nullptr) {
        throw std::bad_alloc();
    } else {
        memset(data, 0, size);
    }
}

void SharedMallocParent::set_size(size_t size) noexcept {
    this->size = size;
}