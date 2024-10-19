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
#include <unistd.h>

#include "popen_wdt.h"

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
    int pipefd_r;  // Subprocess' readfd, write end for the child
    bool running;
};

static pthread_mutex_t wdt_mutex = PTHREAD_MUTEX_INITIALIZER;

// Users should hold mutex
static bool _check_popen_wdt_data(popen_watchdog_data_t **data,
                                  const char *func) {
    bool ret = data && *data && (*data)->privdata;
    POPEN_WDT_DBGLOG("%s: %d. Called from %s", __FUNCTION__, ret, func);
    return ret;
}
#define check_popen_wdt_data(data) _check_popen_wdt_data(data, __FUNCTION__)

static void *watchdog(void *arg) {
    POPEN_WDT_DBGLOG("Watchdog sleeping for %d seconds", SLEEP_SECONDS);
    sleep(SLEEP_SECONDS);

    POPEN_WDT_DBGLOG("++");
    popen_watchdog_data_t *data = (popen_watchdog_data_t *)arg;
    struct popen_wdt_posix_priv *pdata = data->privdata;

    POPEN_WDT_DBGLOG("Check subprocess");
    if (killpg(pdata->childprocess_pid, 0) == 0) {
        if (killpg(pdata->childprocess_pid, SIGINT) == -1) {
            POPEN_WDT_DBGLOG("Failed to send SIGINT to process group: %s",
                             strerror(errno));
        } else {
            POPEN_WDT_DBGLOG("SIGINT signal sent to child process group");
            data->watchdog_activated = true;
        }
    } else {
        POPEN_WDT_DBGLOG("Child process group %d does not exist",
                         pdata->childprocess_pid);
    }

    POPEN_WDT_DBGLOG("--");
    pdata->running = false;
    return NULL;
}

bool popen_watchdog_start(popen_watchdog_data_t **data_in) {
    popen_watchdog_data_t *data = NULL;
    struct popen_wdt_posix_priv *pdata = NULL;
    pthread_mutexattr_t mutexattr;
    pthread_t watchdog_thread = 0;
    int pipefd[2];
    pid_t pid = 0;

    if (data_in == NULL || *data_in == NULL) {
        return false;
    }
    data = *data_in;

    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&wdt_mutex, &mutexattr);
    pthread_mutexattr_destroy(&mutexattr);

    pdata = calloc(1, sizeof(struct popen_wdt_posix_priv));
    if (pdata == NULL) {
        pthread_mutex_destroy(&wdt_mutex);
        return false;
    }

    if (data->watchdog_enabled) {
        POPEN_WDT_DBGLOG("Watchdog is enabled");
    } else {
        POPEN_WDT_DBGLOG("Watchdog disabled");
    }
    if (data->watchdog_enabled) {
        int result = pthread_create(&watchdog_thread, NULL, &watchdog, data);
        if (result != 0) {
            POPEN_WDT_DBGLOG("Failed to create watchdog thread: %s",
                             strerror(result));
            free(pdata);
            free(data);
            *data_in = NULL;
            pthread_mutex_destroy(&wdt_mutex);
            return false;
        }
        pdata->wdt_thread = watchdog_thread;
    }
    data->privdata = pdata;

    if (pipe(pipefd) == -1) {
        free(pdata);
        free(data);
        *data_in = NULL;
        pthread_mutex_destroy(&wdt_mutex);
        return false;
    }

    pid = vfork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        free(pdata);
        free(data);
        *data_in = NULL;
        pthread_mutex_destroy(&wdt_mutex);
        return NULL;
    }

    if (pid == 0) {
        // Force "C" locale to avoid locale-dependent behavior
        setenv("LC_ALL", "C", true);
        // Set process group ID it its pid.
        if (setpgrp() == -1) {
            POPEN_WDT_DBGLOG("Failed to set process group: %s",
                             strerror(errno));
            _exit(127);
        }

        // Close unused file descriptors
        close(pipefd[0]);
        close(STDIN_FILENO);
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
        pdata->childprocess_pid = pid;
        pdata->running = true;
        POPEN_WDT_DBGLOG("Parent pid is %d", getpid());
        POPEN_WDT_DBGLOG("Child pid is %d", pid);
    }
    return true;
}

void popen_watchdog_stop(popen_watchdog_data_t **data_in) {
    pthread_mutex_lock(&wdt_mutex);

    if (!check_popen_wdt_data(data_in)) {
        pthread_mutex_unlock(&wdt_mutex);
        return;
    }
    popen_watchdog_data_t *data = *data_in;
    struct popen_wdt_posix_priv *pdata = data->privdata;

    if (!pdata->running) {
        pthread_mutex_unlock(&wdt_mutex);
        return;
    }
    if (data->watchdog_enabled) {
        POPEN_WDT_DBGLOG("Stopping watchdog");
        pthread_cancel(pdata->wdt_thread);
        pthread_mutex_unlock(&wdt_mutex);
        pthread_join(pdata->wdt_thread, NULL);
    } else {
        pthread_mutex_unlock(&wdt_mutex);
    }
    pdata->running = false;
}

void popen_watchdog_destroy(popen_watchdog_data_t **data_in) {
    popen_watchdog_data_t *data = NULL;
    struct popen_wdt_posix_priv *pdata = NULL;
    int status = 0;

    pthread_mutex_lock(&wdt_mutex);
    if (!check_popen_wdt_data(data_in)) {
        pthread_mutex_unlock(&wdt_mutex);
        return;
    }

    data = *data_in;
    pdata = data->privdata;
    if (waitpid(pdata->childprocess_pid, &status, 0) < 0) {
        POPEN_WDT_DBGLOG("Failed to wait for child process");
    } else {
        if (WIFSIGNALED(status)) {
            POPEN_WDT_DBGLOG("Child process %d exited with signal %d",
                             pdata->childprocess_pid, WTERMSIG(status));
        } else if (WIFEXITED(status)) {
            POPEN_WDT_DBGLOG("Child process %d exited with status %d",
                             pdata->childprocess_pid, WEXITSTATUS(status));
        }
    }
    close(pdata->pipefd_r);
    free(pdata);
    free(data);
    data = NULL;
    pthread_mutex_unlock(&wdt_mutex);
    pthread_mutex_destroy(&wdt_mutex);
    *data_in = NULL;
}

bool popen_watchdog_read(popen_watchdog_data_t **data, char *buf, int size) {
    bool ret = false;
    const int one_sec = 1000;
    struct pollfd fds;
    popen_watchdog_data_t *data_ = NULL;
    struct popen_wdt_posix_priv *pdata = NULL;

    pthread_mutex_lock(&wdt_mutex);

    if (!check_popen_wdt_data(data)) {
        pthread_mutex_unlock(&wdt_mutex);
        return ret;
    }
    data_ = *data;
    pdata = data_->privdata;
    if (data_->watchdog_activated) {
        pthread_mutex_unlock(&wdt_mutex);
        return ret;
    }
    fds.events = POLLIN;
    fds.revents = 0;
    fds.fd = pdata->pipefd_r;
    int pollRet =
        poll(&fds, 1, data_->watchdog_enabled ? SLEEP_SECONDS * one_sec : -1);
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
    pthread_mutex_unlock(&wdt_mutex);
    return ret;
}

bool popen_watchdog_activated(popen_watchdog_data_t **data) {
    bool ret = false;

    if ((*data)->watchdog_enabled) {
        // Wait for joining the thread
        popen_watchdog_stop(data);
    }

    POPEN_WDT_DBGLOG("++");
    pthread_mutex_lock(&wdt_mutex);
    POPEN_WDT_DBGLOG("Locked mutex");

    if (!check_popen_wdt_data(data)) {
        pthread_mutex_unlock(&wdt_mutex);
        return ret;
    }
    POPEN_WDT_DBGLOG("Checking watchdog activated");
    ret = (*data)->watchdog_activated;
    POPEN_WDT_DBGLOG("watchdog activated: %d", ret);
    pthread_mutex_unlock(&wdt_mutex);
    POPEN_WDT_DBGLOG("--");
    return ret;
}
