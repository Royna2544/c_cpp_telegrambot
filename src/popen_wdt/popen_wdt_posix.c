#include <fcntl.h>
#include <locale.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <utils/libutils.h>

#include "popen_wdt.h"

struct watchdog_data {
    pid_t pid;
    bool *result_cb;
};

static void *watchdog(void *arg) {
    struct watchdog_data *data = (struct watchdog_data *)arg;
    sleep(SLEEP_SECONDS);
    *data->result_cb = false;
    if (kill(data->pid, 0) == 0) {
        killpg(data->pid, SIGTERM);
        *data->result_cb = true;
    }
    munmap(data, sizeof(*data));
    return NULL;
}
void setlocale_enus_once(void) {
    setlocale(LC_ALL, "en-US");
}
FILE *popen_watchdog(const char *command, bool *watchdog_ret) {
    FILE *fp;
    int pipefd[2];
    pid_t pid;
    struct watchdog_data *data = NULL;

    if (pipe(pipefd) == -1) {
        return NULL;
    }

    if (watchdog_ret) {
        data = mmap(NULL, sizeof(*data), PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (data == MAP_FAILED) return NULL;

        data->result_cb = watchdog_ret;

        pthread_t watchdog_thread;
        pthread_attr_t attr;

        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&watchdog_thread, &attr, &watchdog, data);
        pthread_attr_destroy(&attr);
    }
    PRETTYF("Command: %s", command);
    pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        // Child process
        if (watchdog_ret)
            data->pid = getpid();
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(STDIN_FILENO);
        setpgid(0, 0);
        execl("/bin/bash", "bash", "-c", command, (char *)NULL);
        _exit(127);  // If execl fails, exit
    } else {
        // Parent process
        close(pipefd[1]);
        fp = fdopen(pipefd[0], "r");
    }

    return fp;
}
