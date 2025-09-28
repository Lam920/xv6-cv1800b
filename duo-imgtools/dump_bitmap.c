#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#define SUPERBLOCK_OFFSET 1024
#define EXT2_SUPER_MAGIC  0xEF53

struct ext2_super_block {
    uint32_t inodes_count;
    uint32_t blocks_count;
    uint32_t r_blocks_count;
    uint32_t free_blocks_count;
    uint32_t free_inodes_count;
    uint32_t first_data_block;
    uint32_t log_block_size;
    uint32_t log_frag_size;
    uint32_t blocks_per_group;
    uint32_t frags_per_group;
    uint32_t inodes_per_group;
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mnt_count;
    uint16_t max_mnt_count;
    uint16_t magic;
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creator_os;
    uint32_t rev_level;
    uint16_t def_resuid;
    uint16_t def_resgid;
    // ... (rest ignored)
};

struct ext2_group_desc {
    uint32_t block_bitmap;      /* Block number of block bitmap */
    uint32_t inode_bitmap;      /* Block number of inode bitmap */
    uint32_t inode_table;       /* Starting block of inode table */
    uint16_t free_blocks_count;
    uint16_t free_inodes_count;
    uint16_t used_dirs_count;
    uint16_t pad;
    uint32_t reserved[3];
};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <device>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct ext2_super_block sb;
    if (pread(fd, &sb, sizeof(sb), SUPERBLOCK_OFFSET) != sizeof(sb)) {
        perror("pread superblock");
        close(fd);
        return 1;
    }

    if (sb.magic != EXT2_SUPER_MAGIC) {
        fprintf(stderr, "Not an ext2 filesystem (magic=0x%04x)\n", sb.magic);
        close(fd);
        return 1;
    }

    unsigned long block_size = 1024U << sb.log_block_size;
    unsigned long groups = (sb.blocks_count + sb.blocks_per_group - 1) / sb.blocks_per_group;

    printf("Block size = %lu\n", block_size);
    printf("Total groups = %lu\n", groups);

    struct ext2_group_desc gd;
    for (unsigned long g = 0; g < groups; g++) {
        off_t gd_offset = (block_size == 1024 ? 2048 : block_size) + g * sizeof(gd);
        if (pread(fd, &gd, sizeof(gd), gd_offset) != sizeof(gd)) {
            perror("pread group desc");
            break;
        }

        printf("Group %lu:\n", g);
        printf("  Block bitmap at block %u\n", gd.block_bitmap);
        printf("  Inode bitmap at block %u\n", gd.inode_bitmap);
        printf("  Inode table starts at block %u\n", gd.inode_table);
    }

    close(fd);
    return 0;
}
