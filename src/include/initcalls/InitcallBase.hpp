#pragma once

#include <absl/log/log.h>
#include "../DurationPoint.hpp"

struct InitcallBase {
    /**
     * @brief A virtual function that will return what is initcall doing
     *
     * @return The name of the initcall's work
     */
    virtual const char* getInitCallName() const = 0;

    DurationPoint onStart() {
        DLOG(INFO) << getInitCallName() << ": +++";
        return DurationPoint();
    }

    void onEnd(DurationPoint& dp) {
        DLOG(INFO) << getInitCallName() << ": --- (" << dp.get().count() << "ms)";
    }
};