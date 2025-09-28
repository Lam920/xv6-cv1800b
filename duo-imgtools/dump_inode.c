#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define SUPERBLOCK_OFFSET 1024
#define EXT2_SUPER_MAGIC  0xEF53
#define EXT2_NDIR_BLOCKS  12   // number of direct blocks in ext2

// Ext2 superblock (truncated)
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
    // ... truncated
};

// Group descriptor (v0)
struct ext2_group_desc {
    uint32_t block_bitmap;
    uint32_t inode_bitmap;
    uint32_t inode_table;
    uint16_t free_blocks_count;
    uint16_t free_inodes_count;
    uint16_t used_dirs_count;
    uint16_t pad;
    uint32_t reserved[3];
};

// Ext2 inode (truncated)
struct ext2_inode {
    uint16_t mode;
    uint16_t uid;
    uint32_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t links_count;
    uint32_t blocks;
    uint32_t flags;
    uint32_t osd1;
    uint32_t block[15];  // 0..11 = direct, 12 = single indirect, etc.
    // truncated
};

static int dev_fd;
static uint32_t block_size = 1024;

void read_bytes(off_t offset, void *buf, size_t size) {
    if (lseek(dev_fd, offset, SEEK_SET) < 0) {
        perror("lseek");
        exit(1);
    }
    if (read(dev_fd, buf, size) != size) {
        perror("read");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <device>\n", argv[0]);
        return 1;
    }

    dev_fd = open(argv[1], O_RDONLY);
    if (dev_fd < 0) {
        perror("open");
        return 1;
    }

    // --- Read superblock ---
    struct ext2_super_block sb;
    read_bytes(SUPERBLOCK_OFFSET, &sb, sizeof(sb));
    if (sb.magic != EXT2_SUPER_MAGIC) {
        fprintf(stderr, "Not an ext2 filesystem (magic=0x%x)\n", sb.magic);
        return 1;
    }
    block_size = 1024 << sb.log_block_size;

    printf("Ext2 detected: block size = %u, inodes per group = %u\n",
           block_size, sb.inodes_per_group);

    // --- Read group descriptor (first group only) ---
    struct ext2_group_desc gd;
    off_t gd_offset = (block_size == 1024) ? 2 * block_size : block_size;
    read_bytes(gd_offset, &gd, sizeof(gd));

    printf("Inode table starts at block %u\n", gd.inode_table);

    // --- Iterate first 100 inodes ---
    struct ext2_inode inode;
    for (int i = 1; i <= 100; i++) {
        off_t inode_offset = (off_t)gd.inode_table * block_size +
                             (i - 1) * sizeof(struct ext2_inode);
        read_bytes(inode_offset, &inode, sizeof(inode));

        // if (inode.size == 0)
        //     continue; // skip unused inode

        printf("Inode %3d (size=%u): ", i, inode.size);
        for (int j = 0; j < EXT2_NDIR_BLOCKS; j++) {
            if (inode.block[j])
                printf("%u ", inode.block[j]);
        }
        printf("\n");
    }

    close(dev_fd);
    return 0;
}
