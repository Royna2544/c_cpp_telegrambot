#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

struct timehms {
    int h;
    int m;
    int s;
    int toSeconds(void) { return 60 * 60 * h + 60 * m + s; }
};

template <typename T>
using callback_t = std::function<void(const T*)>;
template <typename T>
using time_callback_t = std::function<void(const T*, struct timehms)>;
template <typename T>
using cancel_validator_t = std::function<bool(const T*)>;

template <typename T>
class Timer {
    unsigned int h, m, s;
    unsigned int onsec;
    std::unique_ptr<T> priv;
    time_callback_t<T> onEvery;
    callback_t<T> onEnd;
    std::atomic_bool stop;

   public:
    Timer() = delete;
    Timer(unsigned int h, unsigned int m, unsigned int s) : h(h), m(m), s(s){};

    void setCallback(const time_callback_t<T> onEvery, const unsigned int onsec,
                     const callback_t<T> onEnd, const std::unique_ptr<T> priv);
    void start(void);
    void cancel(const cancel_validator_t<T> cancel) {
        if (cancel && cancel(priv.get())) stop = true;
    }
    bool isrunning(void) { return !stop; }
};
