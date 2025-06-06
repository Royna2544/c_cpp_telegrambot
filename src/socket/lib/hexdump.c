#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <ctype.h>

void hexdump(const uint8_t* data, size_t len) {
    const size_t bytes_per_line = 16;

    for (size_t i = 0; i < len; i += bytes_per_line) {
        // Print offset
        printf("%08zx  ", i);

        // Print hex bytes
        for (size_t j = 0; j < bytes_per_line; ++j) {
            if (i + j < len) {
                printf("%02x ", data[i + j]);
            } else {
                printf("   ");
            }
        }

        printf(" ");

        // Print ASCII characters
        for (size_t j = 0; j < bytes_per_line; ++j) {
            if (i + j < len) {
                unsigned char c = data[i + j];
                printf("%c", isprint(c) ? c : '.');
            }
        }

        printf("\n");
    }
}
