#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "popen_wdt.h"
#include "portable_sem.h"

#ifdef POPEN_WDT_DEBUG
#define POPEN_WDT_DBGLOG(fmt, ...)                                       \
    do {                                                                 \
        printf("POPEN_WDT::UNIX: Func %s, Line %d: " fmt "\n", __func__, \
               __LINE__, ##__VA_ARGS__);                                 \
    } while (0)
#else
#define POPEN_WDT_DBGLOG(fmt, ...)
#endif

struct popen_wdt_posix_priv {
    pthread_t wdt_thread;
    pid_t childprocess_pid;
    pthread_cond_t condition;
    pthread_mutex_t wdt_mutex;
    struct rk_sema *startup_sem;
    int status;
    int pipefd_r;  // Subprocess' readfd, write end for the child
    bool process_is_running;
};

static void *watchdog(void *arg) {
    POPEN_WDT_DBGLOG("++");
    popen_watchdog_data_t *data = (popen_watchdog_data_t *)arg;
    struct popen_wdt_posix_priv *pdata = data->privdata;

    struct timespec ts = {};
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += data->sleep_secs;

    POPEN_WDT_DBGLOG("Posting semaphore");
    rk_sema_post(pdata->startup_sem);
    POPEN_WDT_DBGLOG("Done posting semaphore");

    while (pdata->process_is_running ||
           waitpid(pdata->childprocess_pid, &pdata->status, WNOHANG) == 0) {
        pthread_mutex_lock(&pdata->wdt_mutex);
        POPEN_WDT_DBGLOG("Watchdog sleeping for %d seconds", data->sleep_secs);
        int res =
            pthread_cond_timedwait(&pdata->condition, &pdata->wdt_mutex, &ts);
        POPEN_WDT_DBGLOG("Done condwait");
        if (res == ETIMEDOUT) {
            POPEN_WDT_DBGLOG("Watchdog timeout");
            data->watchdog_activated = true;
            if (killpg(pdata->childprocess_pid, POPEN_WDT_SIGTERM) == -1) {
                POPEN_WDT_DBGLOG("Failed to send SIGINT to process group");
            } else {
                POPEN_WDT_DBGLOG("SIGINT signal sent");
                pthread_mutex_unlock(&pdata->wdt_mutex);
                break;
            }
        } else if (res != 0) {
            POPEN_WDT_DBGLOG("pthread_cond_timedwait failed: %d", res);
        }
        pthread_mutex_unlock(&pdata->wdt_mutex);
        if (res == 0) {
            POPEN_WDT_DBGLOG("Watchdog woke up");
            data->watchdog_activated = false;
            break;
        }
    }
    pdata->process_is_running = false;
    POPEN_WDT_DBGLOG("--");
    return NULL;
}

bool popen_watchdog_start(popen_watchdog_data_t **data_in) {
    popen_watchdog_data_t *data = NULL;
    struct popen_wdt_posix_priv *pdata = NULL;
    int pipefd[2];

    if (data_in == NULL || *data_in == NULL) {
        return false;
    }
    data = *data_in;

    pdata = calloc(1, sizeof(struct popen_wdt_posix_priv));
    if (pdata == NULL) {
        return false;
    }

    if (pipe(pipefd) == -1) {
        free(pdata);
        free(data);
        *data_in = NULL;
        return false;
    }

    pthread_mutex_init(&pdata->wdt_mutex, NULL);
    pdata->childprocess_pid = fork();
    if (pdata->childprocess_pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        free(pdata);
        free(data);
        *data_in = NULL;
        pthread_mutex_destroy(&pdata->wdt_mutex);
        return NULL;
    }

    if (pdata->childprocess_pid == 0) {
        // Force "C" locale to avoid locale-dependent behavior
        setenv("LC_ALL", "C", true);

        POPEN_WDT_DBGLOG("Starting subprocess");
        // Set process group ID it its pid.
        if (setpgid(0, 0) == -1) {
            POPEN_WDT_DBGLOG("Failed to set process group: %s",
                             strerror(errno));
            _exit(127);
        }
        POPEN_WDT_DBGLOG("SetPGID done");

        // Close unused file descriptors
        close(pipefd[0]);
        close(STDIN_FILENO);

        POPEN_WDT_DBGLOG("Goodbye stdout");
        // Redirect stdout and stderr to pipe
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        // Call execl to run the command
        execlp("bash", "bash", "-c", data->command, (char *)NULL);
        _exit(127);  // If execl fails, exit
    } else {
        // Parent process
        // Close unused file descriptor
        close(pipefd[1]);
        // Assign privdata members.
        pdata->pipefd_r = pipefd[0];
        pdata->process_is_running = true;
        POPEN_WDT_DBGLOG("Parent pid is %d", getpid());
        POPEN_WDT_DBGLOG("Child pid is %d", pdata->childprocess_pid);
        data->privdata = pdata;

        if (data->watchdog_enabled) {
            struct rk_sema sem;
            rk_sema_init(&sem, 0);
            pdata->startup_sem = &sem;

            POPEN_WDT_DBGLOG("Watchdog is enabled");
            pthread_cond_init(&pdata->condition, NULL);
            if (pthread_create(&pdata->wdt_thread, NULL, &watchdog, data) !=
                0) {
                POPEN_WDT_DBGLOG("Failed to create watchdog thread");
                pthread_mutex_destroy(&pdata->wdt_mutex);
                free(pdata);
                free(data);
                *data_in = NULL;
                return false;
            }
            POPEN_WDT_DBGLOG("Waiting for watchdog thread to start");
            rk_sema_wait(&sem);
            POPEN_WDT_DBGLOG("Watchdog thread started");
            rk_sema_destroy(&sem);
            pdata->startup_sem = NULL;
        } else {
            POPEN_WDT_DBGLOG("Watchdog is disabled");
        }
    }
    return true;
}

static void popen_watchdog_wait(popen_watchdog_data_t **data_in) {
    popen_watchdog_data_t *data = *data_in;
    struct popen_wdt_posix_priv *pdata = data->privdata;

    pthread_mutex_lock(&pdata->wdt_mutex);
    if (!pdata->process_is_running) {
        return;
    }
    pthread_mutex_unlock(&pdata->wdt_mutex);

    if (data->watchdog_enabled) {
        if (waitpid(pdata->childprocess_pid, &pdata->status, 0) < 0) {
            POPEN_WDT_DBGLOG("Failed to wait for child process");
        }
        pdata->process_is_running = false;
        POPEN_WDT_DBGLOG("Child process %d exited, signal cv",
                         pdata->childprocess_pid);
        pthread_cond_signal(&pdata->condition);
        POPEN_WDT_DBGLOG("Waiting for watchdog");
        pthread_join(pdata->wdt_thread, NULL);
        POPEN_WDT_DBGLOG("watchdog joined");
        pthread_cond_destroy(&pdata->condition);
        POPEN_WDT_DBGLOG("Watchdog exited");
    }
}

popen_watchdog_exit_t popen_watchdog_destroy(popen_watchdog_data_t **data_in) {
    popen_watchdog_exit_t ret = POPEN_WDT_EXIT_INITIALIZER;

    if (!data_in || !(*data_in)) {
        return ret;
    }
    popen_watchdog_data_t *data = *data_in;
    struct popen_wdt_posix_priv *pdata = data->privdata;

    if (pdata == NULL) {
        return ret;
    }

    pthread_mutex_lock(&pdata->wdt_mutex);
    if (waitpid(pdata->childprocess_pid, &pdata->status, 0) < 0) {
        POPEN_WDT_DBGLOG("Failed to wait for child process");
    }
    int status = pdata->status;
    if (WIFSIGNALED(status)) {
        POPEN_WDT_DBGLOG("Child process %d exited with signal %d",
                         pdata->childprocess_pid, WTERMSIG(status));
        ret.signal = true;
        ret.exitcode = WTERMSIG(status);
    } else if (WIFEXITED(status)) {
        POPEN_WDT_DBGLOG("Child process %d exited with status %d",
                         pdata->childprocess_pid, WEXITSTATUS(status));
        ret.exitcode = WEXITSTATUS(status);
    }
    close(pdata->pipefd_r);
    pthread_mutex_unlock(&pdata->wdt_mutex);
    pthread_mutex_destroy(&pdata->wdt_mutex);
    free(pdata);
    free(data);
    *data_in = NULL;
    return ret;
}

bool popen_watchdog_read(popen_watchdog_data_t **data_in, char *buf, int size) {
    bool ret = false;
    struct pollfd fds;
    popen_watchdog_data_t *data = *data_in;
    struct popen_wdt_posix_priv *pdata = data->privdata;

    pthread_mutex_lock(&pdata->wdt_mutex);
    int timeout = data->watchdog_enabled ? data->sleep_secs * 1000 : -1;
    fds.events = POLLIN;
    fds.revents = 0;
    fds.fd = pdata->pipefd_r;
    pthread_mutex_unlock(&pdata->wdt_mutex);

    int pollRet = poll(&fds, 1, timeout);
    switch (pollRet) {
        case 0:
            POPEN_WDT_DBGLOG("Timeout...");
            break;
        case -1:
            POPEN_WDT_DBGLOG("poll() failed: %s", strerror(errno));
            break;
        default:
            if (fds.revents & POLLIN) {
                POPEN_WDT_DBGLOG("POLLIN");
                ssize_t readBytes = read(pdata->pipefd_r, buf, size);
                if (readBytes == -1) {
                    POPEN_WDT_DBGLOG("read() failed: %s", strerror(errno));
                } else if (readBytes == 0) {
                    POPEN_WDT_DBGLOG("Child process closed the pipe");
                } else {
                    POPEN_WDT_DBGLOG("read %d bytes: %.*s", (int)readBytes,
                                     (int)readBytes, buf);
                    ret = true;
                }
            } else if (fds.revents & POLLHUP) {
                POPEN_WDT_DBGLOG("POLLHUP");
                ret = false;
            } else {
                POPEN_WDT_DBGLOG("Poll events: %d", fds.revents);
            }
            break;
    }
    return ret;
}

bool popen_watchdog_activated(popen_watchdog_data_t **data_in) {
    bool ret = false;
    popen_watchdog_data_t *data = *data_in;
    struct popen_wdt_posix_priv *pdata = data->privdata;

    if (data->watchdog_enabled) {
        popen_watchdog_wait(data_in);
    }

    POPEN_WDT_DBGLOG("++");
    pthread_mutex_lock(&pdata->wdt_mutex);
    POPEN_WDT_DBGLOG("Checking watchdog activated");
    ret = data->watchdog_activated;
    POPEN_WDT_DBGLOG("watchdog activated: %d", ret);
    pthread_mutex_unlock(&pdata->wdt_mutex);
    POPEN_WDT_DBGLOG("--");
    return ret;
}
