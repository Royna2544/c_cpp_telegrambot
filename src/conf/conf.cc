#include "conf.h"

#include <fcntl.h>
#include <sys/mman.h>
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
        if (flags & O_CREAT) ftruncate(fd, DATA_SIZE);
        mapdata = static_cast<decltype(mapdata)>(
            mmap(NULL, DATA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
        if (mapdata != MAP_FAILED) {
            initdone = true;
        }
	close(fd);
    }
}

bool TgBotConfig::storeToFile(const struct config_data& data) {
    if (!initdone) return false;
    memcpy(mapdata, &data, sizeof(*mapdata));
    msync(mapdata, DATA_SIZE, MS_ASYNC);
    return true;
}

bool TgBotConfig::loadFromFile(struct config_data* data) {
    if (!initdone) return false;
    memcpy(data, mapdata, DATA_SIZE);
    return true;
}

TgBotConfig::~TgBotConfig(void) {
    msync(mapdata, DATA_SIZE, MS_SYNC);
    munmap(mapdata, DATA_SIZE);
}
