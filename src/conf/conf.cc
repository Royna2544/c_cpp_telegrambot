#include "conf.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <sstream>

constexpr size_t DATA_SIZE = sizeof(struct config_data);

TgBotConfig::TgBotConfig(const char* path) {
    auto flags = O_RDWR;
    int fd = open(path, flags);
    if (fd < 0 && errno == ENOENT) {
        flags |= O_CREAT;
        fd = open(path, flags, 0644);
    }
    if (fd > 0) {
        struct stat statbuf;
	void* data;

        if ((flags & O_CREAT) || (fstat(fd, &statbuf) == 0 && statbuf.st_size != DATA_SIZE)) {
            ftruncate(fd, DATA_SIZE);
        }
	data = mmap(NULL, DATA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);;
        if (data == MAP_FAILED) {
            throw std::runtime_error(std::string("Failed to mmap() ") + path + ": " + strerror(errno));
        }
	mapdata = static_cast<decltype(mapdata)>(data);
        if (flags & O_CREAT) memset(mapdata, 0, DATA_SIZE);
        close(fd);
    }
}

void TgBotConfig::storeToFile(const struct config_data& data) noexcept {
    memcpy(mapdata, &data, sizeof(*mapdata));
    msync(mapdata, DATA_SIZE, MS_ASYNC);
}

void TgBotConfig::loadFromFile(struct config_data* data) noexcept {
    if (data == nullptr)
        data = mapdata;
    else
        memcpy(data, mapdata, DATA_SIZE);
}

TgBotConfig::~TgBotConfig(void) noexcept {
    msync(mapdata, DATA_SIZE, MS_SYNC);
    munmap(mapdata, DATA_SIZE);
}
