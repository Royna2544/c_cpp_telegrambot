#include <gtest/gtest.h>

#include <SharedMalloc.hpp>

TEST(SharedMallocTest, UseCount) {
    SharedMalloc shared_malloc(10);
    SharedMalloc some(1000);
    auto v = shared_malloc.getChild();
    {
        auto v2 = shared_malloc.getChild();
        // 3 = shared_malloc parent + v + v2
        ASSERT_EQ(shared_malloc.use_count(), 3);
    }
    // 2 = shared_malloc parent + v
    ASSERT_EQ(shared_malloc.use_count(), 2);
    SharedMallocChild v3 = some.getChild();
    v = std::move(v3);
    // v is gone - so refcnt 1
    EXPECT_EQ(shared_malloc.use_count(), 1);
    EXPECT_EQ(v.get(), some.get());
}

TEST(SharedMallocChildTest, ReturnValidObject) {
    SharedMalloc shared_malloc(10);
    auto child = shared_malloc.getChild();
    EXPECT_NE(child.get(), nullptr);
}

TEST(SharedMallocChildTest, ReturnValidPointer) {
    SharedMalloc shared_malloc(10);
    auto child = shared_malloc.getChild();
    void *ptr = child.get();
    EXPECT_NE(ptr, nullptr);
}