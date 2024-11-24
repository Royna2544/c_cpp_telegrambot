#include "Shmem.hpp"

#include <absl/log/log.h>
#include <fcntl.h>
#include <fmt/core.h>
#include <sys/mman.h>

#include <trivial_helpers/raii.hpp>

AllocatedShmem::AllocatedShmem(const std::string_view& path, off_t size) {
    void* ptr = nullptr;
    int fd = shm_open(path.data(), O_CREAT | O_RDWR, 0666);
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