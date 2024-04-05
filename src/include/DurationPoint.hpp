#pragma once

#include <absl/log/log.h>

#include <chrono>

#include "internal/_std_chrono_templates.h"

struct DurationPoint {
    explicit DurationPoint() { init(); }
    void init() {
        tp = std::chrono::high_resolution_clock::now();
        m_isValid = true;
    }
    std::chrono::milliseconds get() {
        if (m_isValid) {
            m_isValid = false;
            return to_msecs(std::chrono::high_resolution_clock::now() - tp);
        } else {
            LOG(ERROR) << "Timer didn't start yet";
            return std::chrono::milliseconds(0);
        }
    }

   private:
    decltype(std::chrono::high_resolution_clock::now()) tp;
    bool m_isValid = false;
};