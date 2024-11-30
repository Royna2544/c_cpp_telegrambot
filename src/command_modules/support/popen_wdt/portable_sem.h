#pragma once

#include <stdint.h>

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#else
#include <semaphore.h>
#endif

struct rk_sema {
#ifdef __APPLE__
    dispatch_semaphore_t sem;
#else
    sem_t sem;
#endif
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes a semaphore with the given initial value.
 *
 * @param s Pointer to the semaphore structure to be initialized.
 * @param value The initial value for the semaphore.
 *
 * @return void
 */
void rk_sema_init(struct rk_sema *s, uint32_t value);

/**
 * @brief Waits for the semaphore to become available.
 *
 * This function will block the calling thread until the semaphore's value
 * becomes greater than zero. Once the semaphore is available, it will decrement
 * the semaphore's value by one.
 *
 * @param s Pointer to the semaphore structure to be waited on.
 *
 * @return void
 */
void rk_sema_wait(struct rk_sema *s);

/**
 * @brief Increments the semaphore's value by one.
 *
 * This function will wake up one waiting thread if there are any.
 * If no threads are waiting, the semaphore's value will be incremented by one.
 *
 * @param s Pointer to the semaphore structure to be posted.
 *
 * @return void
 */
void rk_sema_post(struct rk_sema *s);

/**
 * @brief Destroys the semaphore and releases any resources associated with it.
 *
 * This function will destroy the semaphore and release any resources associated with it.
 * After calling this function, the semaphore should not be used anymore.
 *
 * @param s Pointer to the semaphore structure to be destroyed.
 *
 * @return void
 */
void rk_sema_destroy(struct rk_sema *s);

#ifdef __cplusplus
}
#endif