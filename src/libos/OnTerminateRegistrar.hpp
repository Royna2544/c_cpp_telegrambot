#include <functional>
#include <map>

#include "InstanceClassBase.hpp"

struct OnTerminateRegistrar : InstanceClassBase<OnTerminateRegistrar> {
    using callback_type = std::function<void()>;

    /**
     * @brief Registers a callback function to be called when the process
     * terminates.
     *
     * @param callback The function to be called when signal is received.
     */
    void registerCallback(const callback_type& callback) {
        callbacks.emplace_back(callback);
    }

    /**
     * @brief Registers a callback function with a token to be called the
     * process terminates.
     *
     * @param callback The function to be called when signal is received.
     * @param token A unique identifier for the callback.
     */
    void registerCallback(const callback_type& callback, const size_t token) {
        callbacksWithToken[token] = callback;
    }

    /**
     * @brief Unregisters a callback function with a token from the list of
     * callbacks to be called when any message is received.
     *
     * @param token The unique identifier of the callback to be unregistered.
     *
     * @return True if the callback with the specified token was found and
     * successfully unregistered, false otherwise.
     */
    bool unregisterCallback(const size_t token) {
        auto it = callbacksWithToken.find(token);
        if (it == callbacksWithToken.end()) {
            return false;
        }
        callbacksWithToken.erase(it);
        return true;
    }

    /**
     * @brief Calls all registered callback functions with the given signal.
     */
    void callCallbacks() {
        for (const auto& callback : callbacks) {
            callback();
        }
        for (const auto& pair : callbacksWithToken) {
            pair.second();
        }
    }

   private:
    std::vector<callback_type> callbacks;
    std::map<size_t, callback_type> callbacksWithToken;
};