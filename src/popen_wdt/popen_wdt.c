#include "popen_wdt.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

struct watchdog_data {
    pid_t pid;
};

static void *watchdog(void *arg) {
    struct watchdog_data *data = (struct watchdog_data *)arg;
    sleep(SLEEP_SECONDS);
    if (kill(data->pid, 0) == 0) {
        kill(data->pid, SIGKILL);
    }
    munmap(data, sizeof(*data));
    return NULL;
}

FILE *popen_watchdog(const char *command) {
    FILE *fp;
    int pipefd[2];
    pid_t pid;
    struct watchdog_data *data;

    if (pipe(pipefd) == -1) {
        return NULL;
    }

    data = mmap(NULL, sizeof(*data), PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (data == NULL) return NULL;

    pthread_t watchdog_thread;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&watchdog_thread, &attr, &watchdog, data);
    pthread_attr_destroy(&attr);

    pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        // Child process
        data->pid = getpid();
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(STDIN_FILENO);
        execl("/bin/bash", "bash", "-c", command, (char *)NULL);
        _exit(127);  // If execl fails, exit
    } else {
        // Parent process
        close(pipefd[1]);
        fp = fdopen(pipefd[0], "r");
    }

    return fp;
}
