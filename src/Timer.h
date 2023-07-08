#pragma once

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
using cancel_validator_t = std::function<bool(T*)>;

template <typename T>
class Timer {
    unsigned int h, m, s;
    unsigned int onsec = 0;
    std::unique_ptr<T> priv;
    time_callback_t<T> onEvery;
    callback_t<T> onEnd;
    bool stop = false;

   public:
    Timer() = delete;
    Timer(unsigned int h, unsigned int m, unsigned int s) : h(h), m(m), s(s){};

    void setCallback(const time_callback_t<T> onEvery, const unsigned int onsec,
                     const callback_t<T> onEnd, const std::unique_ptr<T> priv);
    void start(void);
    bool cancel(const cancel_validator_t<T> cancel) {
        bool shouldcancel = cancel && cancel(priv.get());
        if (shouldcancel) stop = true;
        return shouldcancel;
    }
    void cancel(void) { stop = true; }
    bool isrunning(void) { return !stop; }
};

#include <chrono>

template <typename T>
void Timer<T>::setCallback(const time_callback_t<T> onEvery,
                           const unsigned int onsec, const callback_t<T> onEnd,
                           const std::unique_ptr<T> priv) {
    this->onEvery = onEvery;
    this->onsec = onsec;
    this->onEnd = onEnd;
    this->priv = std::make_unique<T>(*priv);
}

template <typename T>
void Timer<T>::start(void) {
    if (s >= 60) s = 59;
    if (m >= 60) m = 59;
    stop = false;

    std::thread([=]() {
        int h_ = h, m_ = m, s_ = s;

        while (h_ >= 0) {
            while (m_ >= 0) {
                while (s_ >= 0) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    if (stop) break;
                    if (onEvery && s_ % onsec == 0)
                        onEvery(priv.get(), {h_, m_, s_});
                    s_--;
                }
                if (stop) break;
                if (m_ != 0 && s_ == -1) s_ = 60 - onsec;
                m_--;
            }
            if (stop) break;
            if (h_ != 0 && m_ == -1) m_ = 60;
            h_--;
        }
        if (onEnd) onEnd(priv.get());
        stop = true;
    }).detach();
}
