#include "Shmem.hpp"

#include <AbslLogCompat.hpp>
#include <fcntl.h>
#include <fmt/core.h>
#include <sys/mman.h>

#include <trivial_helpers/raii.hpp>

#ifdef __BIONIC__
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#define MAX_SHM_ENTRIES 128  // Max number of named shared memory objects

// Struct to hold fake shared memory objects
typedef struct {
    char name[NAME_MAX];
    int fd;
} shm_entry_t;

static shm_entry_t shm_table[MAX_SHM_ENTRIES];
static pthread_mutex_t shm_mutex = PTHREAD_MUTEX_INITIALIZER;

// Mock shm_open using memfd_create
int shm_open(const char* name, int oflag, mode_t mode) {
    pthread_mutex_lock(&shm_mutex);

    // Check if the name already exists
    for (int i = 0; i < MAX_SHM_ENTRIES; i++) {
        if (shm_table[i].fd != -1 && strcmp(shm_table[i].name, name) == 0) {
            pthread_mutex_unlock(&shm_mutex);
            return shm_table[i].fd;  // Return existing file descriptor
        }
    }

    // Create new memfd
    int fd = syscall(SYS_memfd_create, name, MFD_CLOEXEC);
    if (fd == -1) {
        pthread_mutex_unlock(&shm_mutex);
        return -1;
    }

    // Store the entry
    for (int i = 0; i < MAX_SHM_ENTRIES; i++) {
        if (shm_table[i].fd == -1) {
            strncpy(shm_table[i].name, name, NAME_MAX);
            shm_table[i].fd = fd;
            break;
        }
    }

    pthread_mutex_unlock(&shm_mutex);
    return fd;
}

// Mock shm_unlink
int shm_unlink(const char* name) {
    pthread_mutex_lock(&shm_mutex);

    for (int i = 0; i < MAX_SHM_ENTRIES; i++) {
        if (shm_table[i].fd != -1 && strcmp(shm_table[i].name, name) == 0) {
            close(shm_table[i].fd);
            shm_table[i].fd = -1;
            memset(shm_table[i].name, 0, NAME_MAX);
            pthread_mutex_unlock(&shm_mutex);
            return 0;
        }
    }

    pthread_mutex_unlock(&shm_mutex);
    errno = ENOENT;
    return -1;
}

// Initialization
[[gnu::constructor]] void init_shm_table() {
    for (int i = 0; i < MAX_SHM_ENTRIES; i++) {
        shm_table[i].fd = -1;
    }
}
#endif  // __BIONIC__

AllocatedShmem::AllocatedShmem(const std::string_view& path, off_t size) {
    void* ptr = nullptr;
    int fd = shm_open(path.data(), O_CREAT | O_RDWR,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd == -1) {
        PLOG(ERROR) << "shm_open failed";
        throw syscall_perror("shm_open");
    }
    const auto fdCloser = RAII2<int>::create<int>(fd, close);
    if (ftruncate(fd, size) == -1) {
        PLOG(ERROR) << "ftruncate failed";
        throw syscall_perror("ftruncate");
    }
    ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        PLOG(ERROR) << "mmap failed";
        throw syscall_perror("mmap");
    }
    DLOG(INFO) << fmt::format("Shmem created with path: {} size: {} bytes",
                              path.data(), size);
    this->path = path;
    this->size = size;
    this->memory = ptr;
}

AllocatedShmem::~AllocatedShmem() {
    if (munmap(memory, size) == -1) {
        PLOG(ERROR) << "munmap failed";
    }
    if (shm_unlink(path.data()) == -1) {
        PLOG(ERROR) << "shm_unlink failed";
    }
    DLOG(INFO) << "Shmem freed";
}

ConnectedShmem::ConnectedShmem(const std::string_view& path, const off_t size) {
    int fd = shm_open(path.data(), O_RDWR, 0);
    if (fd == -1) {
        PLOG(ERROR) << "shm_open failed";
        throw syscall_perror("shm_open");
    }
    const auto fdCloser = RAII2<int>::create<int>(fd, close);
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        PLOG(ERROR) << "mmap failed";
        throw syscall_perror("mmap");
    }
    DLOG(INFO) << "Shmem connected with path: " << path.data();
    this->path = path;
    this->size = size;
    this->memory = ptr;
}

ConnectedShmem::~ConnectedShmem() {
    if (memory == nullptr) {
        return;
    }
    if (munmap(memory, size) == -1) {
        PLOG(ERROR) << "munmap failed";
    }
    DLOG(INFO) << "Shmem disconnected";
}
