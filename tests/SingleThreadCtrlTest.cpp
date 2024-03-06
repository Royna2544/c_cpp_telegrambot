#include <gtest/gtest.h>

#include <memory>

#define _SINGLETHREADCTRL_TEST
#include <SingleThreadCtrl.h>

struct SingleThreadCtrlTestAccessors {
    template <class C = SingleThreadCtrl>
    std::shared_ptr<C> createAndGet(int flags = 0) {
        static struct SingleThreadCtrlManager::GetControllerRequest req {
            .usage = SingleThreadCtrlManager::USAGE_TEST
        };
        req.flags = flags;
        return gSThreadManager.getController<C>(req);
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
    SingleThreadCtrlTestAccessors e;
    const int flag = RequireFlagBuilder()
                        .setRequireExist()
                        .setFailActionLog()
                        .build();

    e.createAndAssertNotNull();
    e.createAndAssertNotNull(flag);
    e.destroy();
}

TEST(SingleThreadCtrlTest, RequireNonExistButDoes_FailLog) {
    SingleThreadCtrlTestAccessors e;
    const int flag = RequireFlagBuilder()
                        .setRequireNonExist()
                        .setFailActionLog()
                        .build();

    e.destroy();
    e.createAndAssertNotNull(flag);
    e.destroy();
}

TEST(SingleThreadCtrlTest, RequireExistButDoesnt_FailLog) {
    SingleThreadCtrlTestAccessors e;
    const int flag = RequireFlagBuilder()
                        .setRequireExist()
                        .setFailActionLog()
                        .build();

    e.destroy();
    e.createAndAssertNotNull(flag);
    e.destroy();
}

TEST(SingleThreadCtrlTest, RequireNonExistButDoesnt_FailLog) {
    SingleThreadCtrlTestAccessors e;
    const int flag = RequireFlagBuilder()
                        .setRequireNonExist()
                        .setFailActionLog()
                        .build();

    e.createAndAssertNotNull();
    e.createAndAssertNotNull(flag);
    e.destroy();
}

TEST(SingleThreadCtrlTest, RequireExistButDoesnt_FailLogReturnNull) {
    SingleThreadCtrlTestAccessors e;
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
    SingleThreadCtrlTestAccessors e;
    const int flag = RequireFlagBuilder()
                        .setRequireExist()
                        .setFailActionLog()
                        .setFailActionRetNull()
                        .build();

    e.createAndAssertNotNull();
    e.createAndAssertNotNull(flag);
    e.destroy();
}

#ifndef NDEBUG
TEST(SingleThreadCtrlTest, RequireExistButDoesnt_FailAssert) {
    SingleThreadCtrlTestAccessors e;
    const int flag = RequireFlagBuilder().setRequireExist().setFailActionAssert().build();

    GTEST_FLAG_SET(death_test_style, "threadsafe");
    // As C++ standard require the filename of assert failure on its msg...
    ASSERT_DEATH(e.createAndGet(flag), R"(.*SingleThreadCtrlManager\.cpp.*)");
}

TEST(SingleThreadCtrlTest, RequireNonExistButDoes_FailAssert) {
    SingleThreadCtrlTestAccessors e;
    const int flag = RequireFlagBuilder()
                        .setRequireNonExist()
                        .setFailActionAssert()
                        .build();

    e.createAndAssertNotNull(flag);
}
#endif