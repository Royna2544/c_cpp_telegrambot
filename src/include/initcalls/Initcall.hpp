#pragma once

#include "Logging.h"

struct InitCall {
    /**
     * @brief A virtual function that is called on init
     *
     * @param bot a reference to the bot instance
     */
    virtual void doInitCall(void) = 0;

    /**
     * @brief A virtual function that will return what is initcall doing
     *
     * @return The name of the initcall's work
     */
    virtual const char* getInitCallName() const = 0;

    void initWrapper() {
        LOG(LogLevel::VERBOSE, "%s: +++", getInitCallName());
        doInitCall();
        LOG(LogLevel::VERBOSE, "%s: ---", getInitCallName());
    }
};