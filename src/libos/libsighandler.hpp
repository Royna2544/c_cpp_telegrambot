#pragma once

#include <atomic>

#ifdef WINDOWS_BUILD
#define _WINSOCKAPI_  // stops windows.h including winsock.h
#include <Windows.h>
#endif

class SignalHandler {
   public:
    /**
     * @brief Installs the signal handler.
     *
     * This function installs the signal handler that will handle signals and
     * perform the necessary actions when a signal is received. After calling
     * this function, the signal handler will be active and will handle any
     * signals that are sent to the process.
     *
     * @param None
     *
     * @return None
     *
     * @note This function should be called before any other functions that
     *       rely on the signal handler being active.
     */
    static void install();
    
    /**
     * @brief Uninstalls the signal handler.
     *
     * This function uninstalls the signal handler that was previously
     * installed using the `install` function. After calling this function,
     * the signal handler will no longer be active and will not handle any
     * signals.
     *
     * @param None
     *
     * @return None
     *
     * @note This function should only be called after the signal handler
     *       has been installed using the `install` function. Otherwise, it
     *       will have no effect.
     */
    static void uninstall();

    /**
     * @brief Checks if the signal handler is currently under a signal.
     *
     * This function checks if the signal handler is currently under a
     * signal by loading the value of the atomic boolean variable
     * `kUnderSignal`.
     *
     * @return A boolean value indicating whether the signal handler is
     * currently under a signal. `true` if it is, `false` otherwise.
     */
    static bool isSignaled() { return kUnderSignal.load(); }

#ifdef WINDOWS_BUILD
   friend BOOL WINAPI CtrlHandler(DWORD fdwCtrlType);
#endif
   private:
    static std::atomic_bool kUnderSignal;
    static void signalHandler(int /*signum*/) { signalHandler(); }
    static void signalHandler();
};