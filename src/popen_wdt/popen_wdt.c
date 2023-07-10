#include "popen_wdt.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

struct watchdog_data {
    pid_t pid;
    int pipe_ret;
};

static void *watchdog(void *arg) {
    struct watchdog_data *data = (struct watchdog_data *)arg;
    _Bool ret = 0;
    sleep(SLEEP_SECONDS);
    if (kill(data->pid, 0) == 0) {
        killpg(data->pid, SIGKILL);
	ret = 1;
    }
    if (data->pipe_ret > 0)
	write(data->pipe_ret, &ret, 1);
    munmap(data, sizeof(*data));
    return NULL;
}

FILE *popen_watchdog(const char *command, const int pipe_ret) {
    FILE *fp;
    int pipefd[2];
    pid_t pid;
    struct watchdog_data *data;

    if (pipe(pipefd) == -1) {
        return NULL;
    }

    data = mmap(NULL, sizeof(*data), PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (data == MAP_FAILED) return NULL;

    data->pipe_ret = pipe_ret;

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
