#include "Timer.h"

#include <chrono>

template <typename T>
void Timer<T>::setCallback(const time_callback_t<T> onEvery,
                           const unsigned int onsec, const callback_t<T> onEnd,
                           const std::unique_ptr<T> priv) {
    this->onEvery = onEvery;
    this->onsec = onsec;
    this->onEnd = onEnd;
    this->priv = std::move(priv);
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
                    if (onEvery && s_ % onsec == 0) onEvery(priv, {h_, m_, s_});
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
        if (onEnd) onEnd(priv);
        stop = true;
    }).detach();
}
