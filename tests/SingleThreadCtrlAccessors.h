#pragma once

#include <SingleThreadCtrl.h>
#include <gtest/gtest.h>

template <SingleThreadCtrlManager::ThreadUsage usage>
struct SingleThreadCtrlTestAccessors {
    template <class C = SingleThreadCtrl>
    static std::shared_ptr<C> createAndGet(int flags = 0) {
        static struct SingleThreadCtrlManager::GetControllerRequest req {
            .usage = usage
        };
        req.flags = flags;
        return SingleThreadCtrlManager::getInstance().getController<C>(req);
    }

    static void destroy() {
        SingleThreadCtrlManager::getInstance().destroyController(usage);
    }

    static void createAndAssertNotNull(int flags = 0) {
        AssertNonNull(createAndGet(flags));
    }

    static void AssertNonNull(const std::shared_ptr<SingleThreadCtrl>& p) {
        ASSERT_NE(p.get(), nullptr);
    }
    static void AssertNull(const std::shared_ptr<SingleThreadCtrl>& p) {
        ASSERT_EQ(p.get(), nullptr);
    }
};