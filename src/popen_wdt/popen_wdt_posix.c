#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "popen_wdt.h"

struct popen_wdt_posix_priv {
    pthread_t wdt_thread;
    pid_t wdt_pid;
};

static void *watchdog(void *arg) {
    popen_watchdog_data_t *data = (popen_watchdog_data_t *)arg;
    struct popen_wdt_posix_priv *pdata = data->privdata;

    sleep(SLEEP_SECONDS);
    data->watchdog_activated = false;
    if (kill(pdata->wdt_pid, 0) == 0) {
        killpg(pdata->wdt_pid, SIGTERM);
        data->watchdog_activated = true;
    }
    fclose(data->fp);
    munmap(pdata, sizeof(struct popen_wdt_posix_priv));
    free(data);
    return NULL;
}

bool popen_watchdog_start(popen_watchdog_data_t *data) {
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
        data->fp = fdopen(pipefd[0], "r");
    }
    return true;
}

void popen_watchdog_stop(popen_watchdog_data_t *data) {
    struct popen_wdt_posix_priv *pdata = data->privdata;
    pthread_cancel(pdata->wdt_thread);
    if (data->fp != NULL) {
        fclose(data->fp);
    }
    if (data->watchdog_enabled) {
        data->watchdog_activated = false;
    }
    munmap(pdata, sizeof(struct popen_wdt_posix_priv));
    free(data);
}
