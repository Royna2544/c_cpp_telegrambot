#pragma once

// Like the perror(2)
#ifndef __WIN32
#include <errno.h>
#include <string.h>
#define PLOG_E(fmt, ...) LOG_E(fmt ": %s", ##__VA_ARGS__, strerror(errno))
#else
#define PLOG_E LOG_E
#endif

#define LOG_F(fmt, ...) _LOG(fmt, "FATAL", ##__VA_ARGS__)
#define LOG_E(fmt, ...) _LOG(fmt, "Error", ##__VA_ARGS__)
#define LOG_W(fmt, ...) _LOG(fmt, "Warning", ##__VA_ARGS__)
#define LOG_I(fmt, ...) _LOG(fmt, "Info", ##__VA_ARGS__)
#ifndef NDEBUG
#define LOG_D(fmt, ...) _LOG(fmt, "Debug", ##__VA_ARGS__)
#else
#define LOG_D(fmt, ...) \
    do {                \
    } while (0)
#endif

#define _LOG(fmt, servere, ...) printf("TgBot++: [%s:%d] " servere ": " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
