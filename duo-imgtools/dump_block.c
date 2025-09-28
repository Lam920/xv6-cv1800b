#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define BLOCK_SIZE 1024   // adjust to your ext2 block size (1024, 2048, or 4096)

void dump_block(const unsigned char *buf, size_t size) {
    for (size_t i = 0; i < size; i += 16) {
        printf("%08lx  ", (unsigned long)i);
        for (size_t j = 0; j < 16 && i + j < size; j++) {
            printf("%02x ", buf[i + j]);
        }
        for (size_t j = 16; j > 0 && (i + 16 - j) < size; j--) {
            if (j == 8) printf(" ");
            if (i + 16 - j < size) {
                unsigned char c = buf[i + 16 - j];
                printf("%c", (c >= 32 && c <= 126) ? c : '.');
            }
        }
        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <device> <block_number>\n", argv[0]);
        return 1;
    }

    const char *device = argv[1];
    long blockno = atol(argv[2]);

    int fd = open(device, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Error opening %s: %s\n", device, strerror(errno));
        return 1;
    }

    off_t offset = (off_t)blockno * BLOCK_SIZE;
    if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
        fprintf(stderr, "lseek error: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    unsigned char *buf = malloc(BLOCK_SIZE);
    if (!buf) {
        fprintf(stderr, "malloc failed\n");
        close(fd);
        return 1;
    }

    ssize_t n = read(fd, buf, BLOCK_SIZE);
    if (n != BLOCK_SIZE) {
        fprintf(stderr, "read error: %s\n", strerror(errno));
        free(buf);
        close(fd);
        return 1;
    }

    dump_block(buf, BLOCK_SIZE);

    free(buf);
    close(fd);
    return 0;
}