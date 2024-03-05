#include <gtest/gtest.h>

#include <memory>

#define _SINGLETHREADCTRL_TEST
#include <SingleThreadCtrl.h>

struct SingleThreadCtrlTestAccessors {
    ~SingleThreadCtrlTestAccessors() {
        destroy();
    }
    template <class C = SingleThreadCtrl>
    std::shared_ptr<C> createAndGet(int flags = 0) {
        return gSThreadManager.getController<C>(SingleThreadCtrlManager::USAGE_TEST, flags);
    }

    void destroy() {
        gSThreadManager.destroyController(SingleThreadCtrlManager::USAGE_TEST);
    }

    void createAndAssertNotNull(int flags = 0) {
        AssertNonNull(createAndGet(flags));
    }

    static void AssertNonNull(const std::shared_ptr<SingleThreadCtrl>& p) {
        ASSERT_NE(p.get(), nullptr);
    }
    static void AssertNull(const std::shared_ptr<SingleThreadCtrl>& p) {
        ASSERT_EQ(p.get(), nullptr);
    }
};

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
    int build() {
        int rc = 0;
        if (fail_log)
            rc |= SingleThreadCtrlManager::FLAG_GETCTRL_REQUIRE_FAILACTION_LOG;
        if (fail_assert)
            rc |= SingleThreadCtrlManager::FLAG_GETCTRL_REQUIRE_FAILACTION_ASSERT;
        if (fail_null)
            rc |= SingleThreadCtrlManager::FLAG_GETCTRL_REQUIRE_FAILACTION_RETURN_NULL;
        if (require_exist)
            rc |= SingleThreadCtrlManager::FLAG_GETCTRL_REQUIRE_EXIST;
        if (require_nonexist)
            rc |= SingleThreadCtrlManager::FLAG_GETCTRL_REQUIRE_NONEXIST;
        return rc;
    }
 private:
    bool fail_log = false;
    bool fail_assert = false;
    bool fail_null = false;
    bool require_exist = false;
    bool require_nonexist = false;
};

TEST(SingleThreadCtrlTest, ReturnsNonNull) {
    SingleThreadCtrlTestAccessors e;
    e.createAndAssertNotNull();
}

TEST(SingleThreadCtrlTest, ReturnsSameInstance) {
    SingleThreadCtrlTestAccessors e;
    struct Variable : SingleThreadCtrl {
        bool it = false;
    };
    auto it = e.createAndGet<Variable>();
    e.AssertNonNull(it);
    it->it = true;
    auto it2 = e.createAndGet<Variable>();
    ASSERT_TRUE(it2->it);
}

TEST(SingleThreadCtrlTest, RequireExistButDoesFailLog) {
    SingleThreadCtrlTestAccessors e;
    const int flag = RequireFlagBuilder()
                        .setRequireExist()
                        .setFailActionLog()
                        .build();

    e.createAndAssertNotNull();
    e.createAndAssertNotNull(flag);
}

TEST(SingleThreadCtrlTest, RequireNonExistButDoesFailLog) {
    SingleThreadCtrlTestAccessors e;
    const int flag = RequireFlagBuilder()
                        .setRequireNonExist()
                        .setFailActionLog()
                        .build();

    e.createAndAssertNotNull(flag);
}

TEST(SingleThreadCtrlTest, RequireExistButDoesntFailLog) {
    SingleThreadCtrlTestAccessors e;
    const int flag = RequireFlagBuilder()
                        .setRequireExist()
                        .setFailActionLog()
                        .build();

    e.createAndAssertNotNull(flag);
}

TEST(SingleThreadCtrlTest, RequireNonExistButDoesntFailLog) {
    SingleThreadCtrlTestAccessors e;
    const int flag = RequireFlagBuilder()
                        .setRequireNonExist()
                        .setFailActionLog()
                        .build();

    e.createAndAssertNotNull();
    e.createAndAssertNotNull(flag);
}

TEST(SingleThreadCtrlTest, RequireExistButDoesntFailLogReturnNull) {
    SingleThreadCtrlTestAccessors e;
    const int flag = RequireFlagBuilder()
                        .setRequireExist()
                        .setFailActionLog()
                        .setFailActionRetNull()
                        .build();

    e.AssertNull(e.createAndGet(flag));
}

TEST(SingleThreadCtrlTest, RequireExistButDoesFailLogReturnNull) {
    SingleThreadCtrlTestAccessors e;
    const int flag = RequireFlagBuilder()
                        .setRequireExist()
                        .setFailActionLog()
                        .setFailActionRetNull()
                        .build();

    e.createAndAssertNotNull();
    e.createAndAssertNotNull(flag);
}

#ifndef NDEBUG
TEST(SingleThreadCtrlTest, RequireExistButDoesntFailAssert) {
    SingleThreadCtrlTestAccessors e;
    const int flag = RequireFlagBuilder().setRequireExist().setFailActionAssert().build();

    // As C++ standard require the filename of assert failure on its msg...
    ASSERT_DEATH(e.createAndGet(flag), R"(.*SingleThreadCtrlManager\.cpp.*)");
}

TEST(SingleThreadCtrlTest, RequireNonExistButDoesFailAssert) {
    SingleThreadCtrlTestAccessors e;
    const int flag = RequireFlagBuilder()
                        .setRequireNonExist()
                        .setFailActionAssert()
                        .build();

    e.createAndAssertNotNull();
    // As C++ standard require the filename of assert failure on its msg...
    ASSERT_DEATH(e.createAndGet(flag), R"(.*SingleThreadCtrlManager\.cpp.*)");
}
#endif