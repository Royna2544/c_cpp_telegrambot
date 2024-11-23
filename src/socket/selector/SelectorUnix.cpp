#include <iomanip>
#include <utility>

#include "SelectorPosix.hpp"

UnixSelector::UnixSelector() {
    std::optional<std::string> string = "poll";  // TODO: fix this

    if (string) {
        if (string == "poll") {
            m_selector = std::make_unique<PollSelector>();
        } else if (string == "epoll") {
#ifdef __linux__
            m_selector = std::make_unique<EPollSelector>();
#else
            LOG(ERROR) << "epoll is not supported on this platform";
#endif
        } else if (string == "select") {
            m_selector = std::make_unique<SelectSelector>();
        } else {
            LOG(ERROR) << "Unknown selector: " << std::quoted(*string);
        }
    }
    if (!m_selector) {
        LOG(INFO) << "Config invalid or not set, using default selector: poll";
        m_selector = std::make_unique<PollSelector>();
        string = "poll";
    }
    DLOG(INFO) << "Using selector: " << string.value();
}

bool UnixSelector::init() { return m_selector->init(); }

bool UnixSelector::add(Selector::HandleType fd,
                       Selector::OnSelectedCallback callback,
                       Selector::Mode mode) {
    return m_selector->add(fd, std::move(callback), mode);
}

bool UnixSelector::remove(Selector::HandleType fd) {
    return m_selector->remove(fd);
}

Selector::PollResult UnixSelector::poll() {
    return m_selector->poll();
}

void UnixSelector::shutdown() {
    m_selector->shutdown();
}

bool UnixSelector::reinit() {
    return m_selector->reinit();
}

void UnixSelector::enableTimeout(bool enabled) {
    m_selector->enableTimeout(enabled);
}
