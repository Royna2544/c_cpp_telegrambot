#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <time.h>
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

#define POPEN_WDT_KILL_GRACE_MILLIS    2000
#define POPEN_WDT_KILL_WAIT_MILLIS     2000
#define POPEN_WDT_POLL_INTERVAL_MILLIS 20

struct popen_wdt_posix_priv {
    pthread_t watchdog_thread;
    pthread_cond_t condition;
    pthread_mutex_t mutex;
    pid_t childprocess_pid;
    clockid_t watchdog_clock;
    int status;
    int pipefd_r;
    bool child_reaped;
    bool reaping;
    bool terminating;
    bool watchdog_thread_started;
    bool watchdog_thread_joined;
    bool reap_abandoned;
};

struct detached_reaper_data {
    pid_t pid;
};

static struct timespec monotonic_now(void) {
    struct timespec now = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    return now;
}

static struct timespec clock_after_seconds(clockid_t clock_id, int seconds) {
    struct timespec deadline = {0};
    (void)clock_gettime(clock_id, &deadline);
    if (seconds > 0) {
        deadline.tv_sec += seconds;
    }
    return deadline;
}

static bool deadline_reached(const struct timespec* deadline) {
    const struct timespec now = monotonic_now();
    return now.tv_sec > deadline->tv_sec ||
           (now.tv_sec == deadline->tv_sec && now.tv_nsec >= deadline->tv_nsec);
}

static struct timespec monotonic_after_millis(long millis) {
    struct timespec deadline = monotonic_now();
    deadline.tv_sec += millis / 1000;
    deadline.tv_nsec += (millis % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        ++deadline.tv_sec;
        deadline.tv_nsec -= 1000000000L;
    }
    return deadline;
}

static void sleep_poll_interval(void) {
    const struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = POPEN_WDT_POLL_INTERVAL_MILLIS * 1000000L,
    };
    struct timespec remaining = delay;
    while (nanosleep(&remaining, &remaining) == -1 && errno == EINTR) {
    }
}

static bool set_close_on_exec(int fd) {
    const int flags = fcntl(fd, F_GETFD);
    return flags != -1 && fcntl(fd, F_SETFD, flags | FD_CLOEXEC) != -1;
}

static bool make_output_pipe(int output_pipe[2]) {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__ANDROID__)
    if (pipe2(output_pipe, O_CLOEXEC) == 0) {
        return true;
    }
    if (errno != ENOSYS && errno != EINVAL) {
        return false;
    }
#endif
    if (pipe(output_pipe) == -1) {
        return false;
    }
    if (set_close_on_exec(output_pipe[0]) &&
        set_close_on_exec(output_pipe[1])) {
        return true;
    }
    const int saved_errno = errno;
    (void)close(output_pipe[0]);
    (void)close(output_pipe[1]);
    errno = saved_errno;
    return false;
}

static bool process_group_exists(pid_t pgid) {
    if (killpg(pgid, 0) == 0) {
        return true;
    }
    return errno == EPERM;
}

static bool leader_exists(struct popen_wdt_posix_priv* pdata) {
    pthread_mutex_lock(&pdata->mutex);
    const bool unreaped = !pdata->child_reaped && !pdata->reap_abandoned;
    pthread_mutex_unlock(&pdata->mutex);
    if (!unreaped) {
        return false;
    }
    if (kill(pdata->childprocess_pid, 0) == 0) {
        return true;
    }
    return errno == EPERM;
}

static bool command_processes_exist(struct popen_wdt_posix_priv* pdata) {
    return process_group_exists(pdata->childprocess_pid) ||
           leader_exists(pdata);
}

static void signal_process_tree(struct popen_wdt_posix_priv* pdata,
                                int signal_number) {
    if (killpg(pdata->childprocess_pid, signal_number) == -1 &&
        errno == ESRCH) {
        // The leader may have moved out of its original group. Descendants
        // that deliberately create a new session require stronger OS-level
        // containment, but the directly tracked process is still killable.
        (void)kill(pdata->childprocess_pid, signal_number);
    }
}

static void record_wait_status(struct popen_wdt_posix_priv* pdata, int status) {
    pthread_mutex_lock(&pdata->mutex);
    if (!pdata->child_reaped) {
        pdata->status = status;
        pdata->child_reaped = true;
    }
    pdata->reaping = false;
    pthread_cond_broadcast(&pdata->condition);
    pthread_mutex_unlock(&pdata->mutex);
}

static void release_reaper(struct popen_wdt_posix_priv* pdata) {
    pthread_mutex_lock(&pdata->mutex);
    pdata->reaping = false;
    pthread_cond_broadcast(&pdata->condition);
    pthread_mutex_unlock(&pdata->mutex);
}

static bool claim_reaper(struct popen_wdt_posix_priv* pdata) {
    bool claimed = false;
    pthread_mutex_lock(&pdata->mutex);
    if (!pdata->child_reaped && !pdata->reaping && !pdata->reap_abandoned) {
        pdata->reaping = true;
        claimed = true;
    }
    pthread_mutex_unlock(&pdata->mutex);
    return claimed;
}

static void* detached_reaper(void* arg) {
    struct detached_reaper_data* data = arg;
    const pid_t pid = data->pid;
    free(data);

    pid_t result;
    do {
        result = waitpid(pid, NULL, 0);
    } while (result == -1 && errno == EINTR);
    return NULL;
}

static void abandon_reap(struct popen_wdt_posix_priv* pdata) {
    pthread_mutex_lock(&pdata->mutex);
    pdata->reap_abandoned = true;
    pdata->reaping = false;
    pthread_cond_broadcast(&pdata->condition);
    pthread_mutex_unlock(&pdata->mutex);

    struct detached_reaper_data* data = malloc(sizeof(*data));
    if (data == NULL) {
        return;
    }
    data->pid = pdata->childprocess_pid;
    pthread_t thread;
    if (pthread_create(&thread, NULL, &detached_reaper, data) == 0) {
        (void)pthread_detach(thread);
    } else {
        free(data);
    }
}

static bool reap_child_nohang(struct popen_wdt_posix_priv* pdata) {
    int status = 0;
    const pid_t result = waitpid(pdata->childprocess_pid, &status, WNOHANG);
    if (result == pdata->childprocess_pid) {
        record_wait_status(pdata, status);
        return true;
    }
    if (result == -1 && errno == ECHILD) {
        // Another synchronized waiter already recorded the useful status.
        release_reaper(pdata);
        return true;
    }
    return false;
}

static bool child_finished_nohang(struct popen_wdt_posix_priv* pdata) {
    if (!claim_reaper(pdata)) {
        return false;
    }
    if (reap_child_nohang(pdata)) {
        return true;
    }
    release_reaper(pdata);
    return false;
}

static void wait_for_child(struct popen_wdt_posix_priv* pdata) {
    for (;;) {
        pthread_mutex_lock(&pdata->mutex);
        while (pdata->terminating && !pdata->child_reaped &&
               !pdata->reap_abandoned) {
            pthread_cond_wait(&pdata->condition, &pdata->mutex);
        }
        const bool complete = pdata->child_reaped || pdata->reap_abandoned;
        pthread_mutex_unlock(&pdata->mutex);
        if (complete) {
            return;
        }

        // Never own a blocking waitpid while the watchdog/cancel path may
        // need to become the reaper. Polling leaves that path able to enforce
        // its bounded TERM/KILL deadline even for uninterruptible processes.
        if (child_finished_nohang(pdata)) {
            return;
        }
        sleep_poll_interval();
    }
}

static void terminate_process_group(struct popen_wdt_posix_priv* pdata) {
    const bool owns_reaper = claim_reaper(pdata);

    signal_process_tree(pdata, SIGTERM);

    const struct timespec deadline =
        monotonic_after_millis(POPEN_WDT_KILL_GRACE_MILLIS);
    bool leader_reaped = false;
    while (!deadline_reached(&deadline)) {
        if (owns_reaper && !leader_reaped) {
            leader_reaped = reap_child_nohang(pdata);
        }
        if (!command_processes_exist(pdata)) {
            break;
        }
        sleep_poll_interval();
    }

    if (command_processes_exist(pdata)) {
        signal_process_tree(pdata, SIGKILL);
    }

    if (owns_reaper && !leader_reaped) {
        const struct timespec kill_deadline =
            monotonic_after_millis(POPEN_WDT_KILL_WAIT_MILLIS);
        while (!deadline_reached(&kill_deadline)) {
            if (reap_child_nohang(pdata)) {
                leader_reaped = true;
                break;
            }
            sleep_poll_interval();
        }
        if (!leader_reaped) {
            // An uninterruptible process must not pin a command worker. A
            // detached waiter owns the eventual zombie reap without retaining
            // the watchdog state or blocking destroy().
            abandon_reap(pdata);
        }
    }
}

static void finish_termination(struct popen_wdt_posix_priv* pdata) {
    pthread_mutex_lock(&pdata->mutex);
    pdata->terminating = false;
    pthread_cond_broadcast(&pdata->condition);
    pthread_mutex_unlock(&pdata->mutex);
}

static void* watchdog(void* arg) {
    popen_watchdog_data_t* data = (popen_watchdog_data_t*)arg;
    struct popen_wdt_posix_priv* pdata = data->privdata;
    const struct timespec deadline =
        clock_after_seconds(pdata->watchdog_clock, data->sleep_secs);

    pthread_mutex_lock(&pdata->mutex);
    while (!pdata->child_reaped && !pdata->terminating) {
        const int result =
            pthread_cond_timedwait(&pdata->condition, &pdata->mutex, &deadline);
        if (result == ETIMEDOUT) {
            pdata->terminating = true;
            pthread_mutex_unlock(&pdata->mutex);
            // A process that completed naturally at the deadline must retain
            // its real status. However, descendants that still occupy the
            // command's process group remain part of the timed operation and
            // must not escape merely because the shell leader exited first.
            (void)child_finished_nohang(pdata);
            if (!command_processes_exist(pdata)) {
                finish_termination(pdata);
                return NULL;
            }
            data->watchdog_activated = true;
            terminate_process_group(pdata);
            finish_termination(pdata);
            return NULL;
        }
        if (result != 0) {
            POPEN_WDT_DBGLOG("pthread_cond_timedwait failed: %d", result);
            break;
        }
    }
    pthread_mutex_unlock(&pdata->mutex);
    return NULL;
}

static void cleanup_priv(struct popen_wdt_posix_priv* pdata) {
    if (pdata == NULL) {
        return;
    }
    if (pdata->pipefd_r >= 0) {
        (void)close(pdata->pipefd_r);
    }
    (void)pthread_cond_destroy(&pdata->condition);
    (void)pthread_mutex_destroy(&pdata->mutex);
    free(pdata);
}

bool popen_watchdog_start(popen_watchdog_data_t** data_in) {
    if (data_in == NULL || *data_in == NULL || (*data_in)->command == NULL) {
        return false;
    }

    popen_watchdog_data_t* data = *data_in;
    struct popen_wdt_posix_priv* pdata = calloc(1, sizeof(*pdata));
    if (pdata == NULL) {
        return false;
    }
    pdata->pipefd_r = -1;
    if (pthread_mutex_init(&pdata->mutex, NULL) != 0) {
        free(pdata);
        return false;
    }
    pthread_condattr_t condition_attr;
    const bool condition_attr_initialized =
        pthread_condattr_init(&condition_attr) == 0;
    pdata->watchdog_clock = CLOCK_REALTIME;
#if defined(__linux__) || defined(__FreeBSD__) || defined(__ANDROID__)
    if (condition_attr_initialized &&
        pthread_condattr_setclock(&condition_attr, CLOCK_MONOTONIC) == 0) {
        pdata->watchdog_clock = CLOCK_MONOTONIC;
    }
#endif
    if (pthread_cond_init(&pdata->condition, condition_attr_initialized
                                                 ? &condition_attr
                                                 : NULL) != 0) {
        if (condition_attr_initialized) {
            (void)pthread_condattr_destroy(&condition_attr);
        }
        (void)pthread_mutex_destroy(&pdata->mutex);
        free(pdata);
        return false;
    }
    if (condition_attr_initialized) {
        (void)pthread_condattr_destroy(&condition_attr);
    }

    int output_pipe[2] = {-1, -1};
    if (!make_output_pipe(output_pipe)) {
        cleanup_priv(pdata);
        return false;
    }

    pdata->childprocess_pid = fork();
    if (pdata->childprocess_pid == -1) {
        (void)close(output_pipe[0]);
        (void)close(output_pipe[1]);
        cleanup_priv(pdata);
        return false;
    }

    if (pdata->childprocess_pid == 0) {
        (void)close(output_pipe[0]);
        if (setpgid(0, 0) == -1 || dup2(output_pipe[1], STDOUT_FILENO) == -1 ||
            dup2(output_pipe[1], STDERR_FILENO) == -1) {
            _exit(POPEN_WDT_EXIT_CODE_MAX);
        }
        (void)fcntl(STDOUT_FILENO, F_SETFD, 0);
        (void)fcntl(STDERR_FILENO, F_SETFD, 0);
        if (output_pipe[1] != STDOUT_FILENO &&
            output_pipe[1] != STDERR_FILENO) {
            (void)close(output_pipe[1]);
        }
        const int nullfd = open("/dev/null", O_RDONLY);
        if (nullfd >= 0) {
            (void)dup2(nullfd, STDIN_FILENO);
            if (nullfd != STDIN_FILENO) {
                (void)close(nullfd);
            }
        }
        execlp(POPEN_WDT_DEFAULT_SHELL, POPEN_WDT_DEFAULT_SHELL, "-c",
               data->command, (char*)NULL);
        _exit(POPEN_WDT_EXIT_CODE_MAX);
    }

    (void)close(output_pipe[1]);
    pdata->pipefd_r = output_pipe[0];
    data->privdata = pdata;

    // Close the fork/setpgid race: either side may establish the group first.
    if (setpgid(pdata->childprocess_pid, pdata->childprocess_pid) == -1 &&
        errno != EACCES && errno != ESRCH) {
        POPEN_WDT_DBGLOG("parent setpgid failed: %s", strerror(errno));
    }

    if (data->watchdog_enabled) {
        if (pthread_create(&pdata->watchdog_thread, NULL, &watchdog, data) !=
            0) {
            pthread_mutex_lock(&pdata->mutex);
            pdata->terminating = true;
            pthread_mutex_unlock(&pdata->mutex);
            terminate_process_group(pdata);
            finish_termination(pdata);
            wait_for_child(pdata);
            cleanup_priv(pdata);
            data->privdata = NULL;
            return false;
        }
        pdata->watchdog_thread_started = true;
    }
    return true;
}

static void join_watchdog(struct popen_wdt_posix_priv* pdata) {
    if (pdata->watchdog_thread_started && !pdata->watchdog_thread_joined) {
        (void)pthread_join(pdata->watchdog_thread, NULL);
        pdata->watchdog_thread_joined = true;
    }
}

bool popen_watchdog_cancel(popen_watchdog_data_t** data_in) {
    if (data_in == NULL || *data_in == NULL || (*data_in)->privdata == NULL) {
        return false;
    }
    popen_watchdog_data_t* data = *data_in;
    struct popen_wdt_posix_priv* pdata = data->privdata;

    pthread_mutex_lock(&pdata->mutex);
    if (pdata->child_reaped) {
        pthread_mutex_unlock(&pdata->mutex);
        return false;
    }
    if (pdata->terminating) {
        while (pdata->terminating) {
            pthread_cond_wait(&pdata->condition, &pdata->mutex);
        }
        pthread_mutex_unlock(&pdata->mutex);
        return true;
    }
    pdata->terminating = true;
    data->watchdog_activated = true;
    pthread_cond_broadcast(&pdata->condition);
    pthread_mutex_unlock(&pdata->mutex);

    terminate_process_group(pdata);
    finish_termination(pdata);
    return true;
}

static popen_watchdog_exit_t make_exit_status(
    const struct popen_wdt_posix_priv* pdata) {
    popen_watchdog_exit_t result = POPEN_WDT_EXIT_INITIALIZER;
    if (WIFSIGNALED(pdata->status)) {
        result.signal = true;
        result.exitcode = (popen_watchdog_exit_code_t)WTERMSIG(pdata->status);
    } else if (WIFEXITED(pdata->status)) {
        result.exitcode =
            (popen_watchdog_exit_code_t)WEXITSTATUS(pdata->status);
    }
    return result;
}

popen_watchdog_exit_t popen_watchdog_destroy(popen_watchdog_data_t** data_in) {
    popen_watchdog_exit_t result = POPEN_WDT_EXIT_INITIALIZER;
    if (data_in == NULL || *data_in == NULL) {
        return result;
    }

    popen_watchdog_data_t* data = *data_in;
    struct popen_wdt_posix_priv* pdata = data->privdata;
    if (pdata != NULL) {
        wait_for_child(pdata);
        // Closing a completed shell command is also a containment boundary:
        // reap/terminate background descendants that retained the group even
        // when the primary shell already returned a successful status.
        if (process_group_exists(pdata->childprocess_pid)) {
            terminate_process_group(pdata);
        }
        pthread_mutex_lock(&pdata->mutex);
        pthread_cond_broadcast(&pdata->condition);
        pthread_mutex_unlock(&pdata->mutex);
        join_watchdog(pdata);
        if (pdata->child_reaped) {
            result = make_exit_status(pdata);
        }
        cleanup_priv(pdata);
    }
    free(data);
    *data_in = NULL;
    return result;
}

popen_watchdog_ssize_t popen_watchdog_read(popen_watchdog_data_t** data_in,
                                           char* buf,
                                           popen_watchdog_ssize_t size) {
    if (data_in == NULL || *data_in == NULL || (*data_in)->privdata == NULL ||
        buf == NULL || size <= 0) {
        return -1;
    }
    popen_watchdog_data_t* data = *data_in;
    struct popen_wdt_posix_priv* pdata = data->privdata;
    struct pollfd fd = {
        .fd = pdata->pipefd_r,
        .events = POLLIN,
        .revents = 0,
    };
    const int timeout = data->watchdog_enabled ? data->sleep_secs * 1000 : -1;

    int poll_result;
    do {
        poll_result = poll(&fd, 1, timeout);
    } while (poll_result == -1 && errno == EINTR);
    if (poll_result <= 0) {
        return -1;
    }
    if ((fd.revents & POLLIN) != 0) {
        ssize_t bytes;
        do {
            bytes = read(pdata->pipefd_r, buf, (size_t)size);
        } while (bytes == -1 && errno == EINTR);
        return bytes > 0 ? (popen_watchdog_ssize_t)bytes : -1;
    }
    return -1;
}

bool popen_watchdog_activated(popen_watchdog_data_t** data_in) {
    if (data_in == NULL || *data_in == NULL || (*data_in)->privdata == NULL) {
        return false;
    }
    popen_watchdog_data_t* data = *data_in;
    struct popen_wdt_posix_priv* pdata = data->privdata;
    wait_for_child(pdata);
    pthread_mutex_lock(&pdata->mutex);
    pthread_cond_broadcast(&pdata->condition);
    pthread_mutex_unlock(&pdata->mutex);
    join_watchdog(pdata);
    return data->watchdog_activated;
}
