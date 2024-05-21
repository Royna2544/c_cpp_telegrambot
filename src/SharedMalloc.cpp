#include <absl/log/log.h>

#include <SharedMalloc.hpp>
#include <memory>

SharedMallocChild::SharedMallocChild(std::shared_ptr<SharedMallocParent> _parent)
    : parent(std::move(_parent)), m_data(parent->data.get()) {}

SharedMallocChild SharedMalloc::getChild() const noexcept {
    return SharedMallocChild(parent);
}

void SharedMallocParent::alloc() {
    if (data == nullptr) {
        data.reset(malloc(size));
    } else {
        data.reset(realloc(data.release(), size));
    }
    if (data == nullptr) {
        throw std::bad_alloc();
    }
}
