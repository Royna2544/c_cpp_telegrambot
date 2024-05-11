#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "popen_wdt.h"

struct popen_wdt_posix_priv {
    pthread_t wdt_thread;
    pid_t wdt_pid;
    FILE *fp;
    bool stopped;
    _Atomic int refcount;
};

static void free_popen_wdt_data(popen_watchdog_data_t *data) {
    struct popen_wdt_posix_priv *pdata = data->privdata;

    printf("Note: %s\n", __func__);
    if (pdata->refcount == 0) {
        puts("Note: freeing popen_watchdog_data_t");
        if (data->watchdog_activated) {
            munmap(pdata, sizeof(struct popen_wdt_posix_priv));
        } else {
            free(pdata);
        }
        free(data);
    }
}

static void *watchdog(void *arg) {
    popen_watchdog_data_t *data = (popen_watchdog_data_t *)arg;
    struct popen_wdt_posix_priv *pdata = data->privdata;

    ++pdata->refcount;
    sleep(SLEEP_SECONDS);
    data->watchdog_activated = false;
    if (kill(pdata->wdt_pid, 0) == 0) {
        killpg(pdata->wdt_pid, SIGTERM);
        data->watchdog_activated = true;
    }
    --pdata->refcount;
    free_popen_wdt_data(data);
    return NULL;
}

bool popen_watchdog_start(popen_watchdog_data_t **data_in) {
    popen_watchdog_data_t *data = *data_in;
    struct popen_wdt_posix_priv *pdata = NULL;
    int pipefd[2];
    pid_t pid = 0;

    if (data == NULL) {
        return false;
    }

    if (pipe(pipefd) == -1) {
        return false;
    }

    if (data->watchdog_enabled) {
        pdata = mmap(NULL, sizeof(struct popen_wdt_posix_priv),
                     PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (pdata == MAP_FAILED) {
            return false;
        }

        pthread_t watchdog_thread = 0;
        pthread_attr_t attr;

        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&watchdog_thread, &attr, &watchdog, data);
        pthread_attr_destroy(&attr);
        pdata->wdt_thread = watchdog_thread;
        data->privdata = pdata;
    } else {
        pdata = malloc(sizeof(struct popen_wdt_posix_priv));
        if (pdata == NULL) {
            return false;
        }
        memset(pdata, 0, sizeof(struct popen_wdt_posix_priv));
        data->privdata = pdata;
    }
    pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    setenv("LC_ALL", "C", true);

    if (pid == 0) {
        // Child process
        if (data->watchdog_enabled) {
            pdata->wdt_pid = getpid();
        }
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(STDIN_FILENO);
        setpgid(0, 0);
        execl(BASH_EXE_PATH, "bash", "-c", data->command, (char *)NULL);
        _exit(127);  // If execl fails, exit
    } else {
        // Parent process
        close(pipefd[1]);
        pdata->fp = fdopen(pipefd[0], "r");
    }
    return true;
}

void popen_watchdog_stop(popen_watchdog_data_t **data_in) {
    popen_watchdog_data_t *data = *data_in;
    struct popen_wdt_posix_priv *pdata = data->privdata;
    pdata->refcount++;
    pthread_cancel(pdata->wdt_thread);
    pdata->stopped = true;
    pdata->refcount--;
    free_popen_wdt_data(data);
}

void popen_watchdog_destroy(popen_watchdog_data_t **data_in) {
    if (data_in == NULL) {
        return;
    }
    popen_watchdog_data_t *data = *data_in;
    if (data->privdata == NULL) {
        return;
    }
    struct popen_wdt_posix_priv *pdata = data->privdata;
    pdata->refcount++;
    if (data->watchdog_enabled) {
        data->watchdog_activated = false;
    }
    fclose(pdata->fp);
    if (data->watchdog_enabled) {
        if (!pdata->stopped) {
            popen_watchdog_stop(data_in);
        }
    }
    pdata->refcount--;
    free_popen_wdt_data(data);
    *data_in = NULL;
}

bool popen_watchdog_read(popen_watchdog_data_t **data, char *buf, int size) {
    if (data == NULL) {
        return false;
    }
    popen_watchdog_data_t *data_ = *data;
    if (data_ == NULL) {
        return false;
    }
    if (data_->privdata == NULL) {
        return false;
    }
    struct popen_wdt_posix_priv *pdata = data_->privdata;
    if (data_->watchdog_activated) {
        return false;
    }
    return fgets(buf, size, pdata->fp) != NULL;
}