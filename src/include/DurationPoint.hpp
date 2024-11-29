#pragma once

#include <absl/log/log.h>

#include <chrono>

#include "trivial_helpers/_std_chrono_templates.h"

template <typename Step>
struct DurationPoint {
    explicit DurationPoint() { init(); }
    void init() {
        tp = std::chrono::high_resolution_clock::now();
        m_isValid = true;
    }
    Step get() {
        if (m_isValid) {
            m_isValid = false;
            return std::chrono::duration_cast<Step>(std::chrono::high_resolution_clock::now() - tp);
        } else {
            LOG(ERROR) << "Timer didn't start yet";
            return Step::min();
        }
    }

   private:
    std::chrono::high_resolution_clock::time_point tp;
    bool m_isValid = false;
};

using MilliSecondDP = DurationPoint<std::chrono::milliseconds>;
using SecondDP = DurationPoint<std::chrono::seconds>;