#include <ConfigManager.h>

#include <iomanip>
#include <type_traits>
#include <variant>

#include "SelectorPosix.hpp"

UnixSelector::UnixSelector() {
    auto string = ConfigManager::getVariable(ConfigManager::Configs::SELECTOR);
    bool found = false;

    if (string) {
        if (string == "poll") {
            m_selector = PollSelector();
            found = true;
        } else if (string == "epoll") {
#ifdef __linux__
            m_selector = EPollSelector();
            found = true;
#else
            LOG(ERROR) << "epoll is not supported on this platform";
#endif
        } else if (string == "select") {
            m_selector = SelectSelector();
            found = true;
        } else {
            LOG(ERROR) << "Unknown selector: " << std::quoted(*string);
        }
    }
    if (!found) {
        LOG(INFO) << "Config invalid or not set, using default selector: poll";
        m_selector = PollSelector();
        string = "poll";
    }
    DLOG(INFO) << "Using selector: " << string.value();
}

bool UnixSelector::init() {
    return std::visit(
        [](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (isKnownSelector<T>()) {
                return arg.init();
            }
        },
        m_selector);
}

bool UnixSelector::add(socket_handle_t fd,
                       Selector::OnSelectedCallback callback,
                       Selector::Mode mode) {
    return std::visit(
        [fd, callback, mode](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (isKnownSelector<T>()) {
                return arg.add(fd, callback, mode);
            }
        },
        m_selector);
}

bool UnixSelector::remove(socket_handle_t fd) {
    return std::visit(
        [fd](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (isKnownSelector<T>()) {
                return arg.remove(fd);
            }
        },
        m_selector);
}

Selector::SelectorPollResult UnixSelector::poll() {
    return std::visit(
        [](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (isKnownSelector<T>()) {
                return arg.poll();
            }
        },
        m_selector);
}

void UnixSelector::shutdown() {
    return std::visit(
        [](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (isKnownSelector<T>()) {
                return arg.shutdown();
            }
        },
        m_selector);
}

bool UnixSelector::reinit() {
    return std::visit(
        [](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (isKnownSelector<T>()) {
                return arg.reinit();
            }
        },
        m_selector);
}

void UnixSelector::enableTimeout(bool enabled) {
    return std::visit(
        [enabled](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (isKnownSelector<T>()) {
                return arg.enableTimeout(enabled);
            }
        },
        m_selector);
}
