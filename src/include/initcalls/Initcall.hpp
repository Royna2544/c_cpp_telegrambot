#pragma once

#include "InitcallBase.hpp"

struct InitCall : InitcallBase {
    /**
     * @brief A virtual function that is called on init
     *
     * @param bot a reference to the bot instance
     */
    virtual void doInitCall(void) = 0;

    void initWrapper() {
        auto dp = onStart();
        doInitCall();
        onEnd(dp);
    }
};