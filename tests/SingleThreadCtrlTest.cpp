#include <gtest/gtest.h>

#include "SingleThreadCtrl.h"
#include "SingleThreadCtrlAccessors.h"

using Accessor = SingleThreadCtrlTestAccessors<SingleThreadCtrlManager::USAGE_TEST>;

struct RequireFlagBuilder {
    RequireFlagBuilder& setFailActionLog() {
        fail_log = true;
        fail_assert = false;
        return *this;
    }
    RequireFlagBuilder& setFailActionAssert() {
        fail_log = false;
        fail_assert = true;
        return *this;
    }
    RequireFlagBuilder& setFailActionRetNull() {
        fail_null = true;
        return *this;
    }
    RequireFlagBuilder& setRequireExist() {
        require_nonexist = false;
        require_exist = true;
        return *this;
    }
    RequireFlagBuilder& setRequireNonExist() {
        require_exist = false;
        require_nonexist = true;
        return *this;
    }
    RequireFlagBuilder& setSizediffReconstruct() {
        sizediff_recst = true;
        return *this;
    }
    int build() {
        int rc = 0;
        if (fail_log)
            rc |= SingleThreadCtrlManager::REQUIRE_FAILACTION_LOG;
        if (fail_assert)
            rc |= SingleThreadCtrlManager::REQUIRE_FAILACTION_ASSERT;
        if (fail_null)
            rc |= SingleThreadCtrlManager::REQUIRE_FAILACTION_RETURN_NULL;
        if (require_exist)
            rc |= SingleThreadCtrlManager::REQUIRE_EXIST;
        if (require_nonexist)
            rc |= SingleThreadCtrlManager::REQUIRE_NONEXIST;
        if (sizediff_recst)
            rc |= SingleThreadCtrlManager::SIZEDIFF_ACTION_RECONSTRUCT;
        return rc;
    }
 private:
    bool fail_log = false;
    bool fail_assert = false;
    bool fail_null = false;
    bool require_exist = false;
    bool require_nonexist = false;
    bool sizediff_recst = false;
};

TEST(SingleThreadCtrlTest, ReturnsNonNull) {
    Accessor e;
    e.createAndAssertNotNull();
}

TEST(SingleThreadCtrlTest, ReturnsSameInstance) {
    Accessor e;
    struct Variable : SingleThreadCtrl {
        bool it = false;
        ~Variable() override = default;
    };
    e.destroy();
    auto it = e.createAndGet<Variable>();
    e.AssertNonNull(it);
    it->it = true;
    auto it2 = e.createAndGet<Variable>();
    ASSERT_TRUE(it2->it);
    e.destroy();
}

TEST(SingleThreadCtrlTest, RequireExistButDoes_FailLog) {
    Accessor e;
    const int flag = RequireFlagBuilder()
                        .setRequireExist()
                        .setFailActionLog()
                        .build();

    e.createAndAssertNotNull();
    e.createAndAssertNotNull(flag);
    e.destroy();
}

TEST(SingleThreadCtrlTest, RequireNonExistButDoes_FailLog) {
    Accessor e;
    const int flag = RequireFlagBuilder()
                        .setRequireNonExist()
                        .setFailActionLog()
                        .build();

    e.destroy();
    e.createAndAssertNotNull(flag);
    e.destroy();
}

TEST(SingleThreadCtrlTest, RequireExistButDoesnt_FailLog) {
    Accessor e;
    const int flag = RequireFlagBuilder()
                        .setRequireExist()
                        .setFailActionLog()
                        .build();

    e.destroy();
    e.createAndAssertNotNull(flag);
    e.destroy();
}

TEST(SingleThreadCtrlTest, RequireNonExistButDoesnt_FailLog) {
    Accessor e;
    const int flag = RequireFlagBuilder()
                        .setRequireNonExist()
                        .setFailActionLog()
                        .build();

    e.createAndAssertNotNull();
    e.createAndAssertNotNull(flag);
    e.destroy();
}

TEST(SingleThreadCtrlTest, RequireExistButDoesnt_FailLogReturnNull) {
    Accessor e;
    const int flag = RequireFlagBuilder()
                        .setRequireExist()
                        .setFailActionLog()
                        .setFailActionRetNull()
                        .build();

    e.destroy();
    e.AssertNull(e.createAndGet(flag));
    e.destroy();
}

TEST(SingleThreadCtrlTest, RequireExistButDoes_FailLogReturnNull) {
    Accessor e;
    const int flag = RequireFlagBuilder()
                        .setRequireExist()
                        .setFailActionLog()
                        .setFailActionRetNull()
                        .build();

    e.createAndAssertNotNull();
    e.createAndAssertNotNull(flag);
    e.destroy();
}

struct Amazing : SingleThreadCtrl {
    char buf[2048];
};

TEST(SingleThreadCtrlTest, SizeDiffReconstructYes) {
    Accessor e;

    const int flag = RequireFlagBuilder()
                        .setRequireExist()
                        .setSizediffReconstruct()
                        .build();

    e.createAndAssertNotNull();
    e.AssertNonNull(e.createAndGet<Amazing>(flag));
    e.destroy();
}

TEST(SingleThreadCtrlTest, SizeDiffReconstructNo) {
    Accessor e;

    const int flag = RequireFlagBuilder()
                        .setRequireExist()
                        .build();

    e.createAndAssertNotNull();
    e.AssertNull(e.createAndGet<Amazing>(flag));
    e.destroy();
}

#ifndef NDEBUG
TEST(SingleThreadCtrlTest, RequireExistButDoesnt_FailAssert) {
    Accessor e;
    const int flag = RequireFlagBuilder().setRequireExist().setFailActionAssert().build();

    GTEST_FLAG_SET(death_test_style, "threadsafe");
    // As C++ standard require the filename of assert failure on its msg...
    ASSERT_DEATH(e.createAndGet(flag), R"(.*SingleThreadCtrlManager\.cpp.*)");
}

TEST(SingleThreadCtrlTest, RequireNonExistButDoes_FailAssert) {
    Accessor e;
    const int flag = RequireFlagBuilder()
                        .setRequireNonExist()
                        .setFailActionAssert()
                        .build();

    e.createAndAssertNotNull(flag);
}
#endif