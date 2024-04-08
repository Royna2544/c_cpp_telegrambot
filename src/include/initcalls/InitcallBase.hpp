#pragma once

#include <absl/log/log.h>
#include "../DurationPoint.hpp"
#include "../CStringLifetime.h"

struct InitcallBase {
    /**
     * @brief A virtual function that will return what is initcall doing
     *
     * @return The name of the initcall's work
     */
    virtual const CStringLifetime getInitCallName() const = 0;

    DurationPoint onStart() {
        DLOG(INFO) << getInitCallName().get() << ": +++";
        return DurationPoint();
    }

    void onEnd(DurationPoint& dp) {
        DLOG(INFO) << getInitCallName().get() << ": --- (" << dp.get().count() << "ms)";
    }
};