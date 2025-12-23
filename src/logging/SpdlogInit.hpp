#pragma once

/**
 * Initializes the spdlog Logging library for the TgBot library.
 * This function should be called before using any other TgBot functions that rely on logging.
 *
 * @return void
 *
 * @note This function is expected to be called only once during the initialization of the application.
 * @note The spdlog Logging library is used for logging within the TgBot library.
 * @note This function does not perform any error checking or handle exceptions.
 * @note The behavior of the TgBot library is undefined if this function is not called before using other TgBot functions.
 */
extern void TgBot_SpdlogInit();

// Deregister and cleanup spdlog
extern void TgBot_SpdlogDeInit();
