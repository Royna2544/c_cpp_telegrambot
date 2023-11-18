#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <thread>

struct timehms {
    int h, m, s;
    int toSeconds(void) const noexcept { return 60 * 60 * h + 60 * m + s; }
};

template <typename T>
using callback_t = std::function<void(const T*)>;
template <typename T>
using time_callback_t = std::function<void(const T*, struct timehms)>;
template <typename T>
using cancel_validator_t = std::function<bool(T*)>;

using std::chrono_literals::operator""s;

template <class T>
class Timer {
    int h, m, s;
    unsigned int onsec = 0;
    T priv {};
    time_callback_t<T> onEvery;
    callback_t<T> onEnd;
    bool stop = false;

   public:
    Timer() = delete;
    Timer(int h, int m, int s) : h(h), m(m), s(s){};

    bool cancel(const cancel_validator_t<T> cancel) {
        bool shouldcancel = cancel && cancel(&priv);
        if (shouldcancel) stop = true;
        return shouldcancel;
    }
    void cancel(void) { stop = true; }
    bool isrunning(void) { return !stop; }
    void setCallback(const time_callback_t<T> onEvery,
                     const unsigned int onsec, const callback_t<T> onEnd,
                     const T priv) {
        this->onEvery = onEvery;
        this->onsec = onsec;
        this->onEnd = onEnd;
        this->priv = priv;
    }

    void start(void) {
        if (s >= 60) s = 59;
        if (m >= 60) m = 59;
        stop = false;

        std::thread([=]() {
            int h_ = h, m_ = m, s_ = s;

            while (h_ >= 0) {
                while (m_ >= 0) {
                    while (s_ >= 0) {
                        std_sleep(1s);
                        if (stop) break;
                        if (onEvery && s_ % onsec == 0)
                            onEvery(&priv, {h_, m_, s_});
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
            if (onEnd) onEnd(&priv);
            stop = true;
        }).detach();
    }
};
