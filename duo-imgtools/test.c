#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// On-disk EXT2 structures (little endian)
struct ext2_super_block {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algo_bitmap;
};

struct ext2_group_desc {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
};

struct ext2_inode {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[15];
};

struct ext2_dir_entry_2 {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[];
};

#define EXT2_SUPER_MAGIC 0xEF53

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s rootfs.ext2\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }

    struct ext2_super_block sb;
    lseek(fd, 1024, SEEK_SET);
    if (read(fd, &sb, sizeof(sb)) != sizeof(sb)) {
        perror("read superblock");
        return 1;
    }
    if (sb.s_magic != EXT2_SUPER_MAGIC) {
        fprintf(stderr, "Not an ext2 filesystem (magic=%x)\n", sb.s_magic);
        return 1;
    }

    uint32_t block_size = 1024U << sb.s_log_block_size;
    uint32_t inode_size = sb.s_inode_size ? sb.s_inode_size : 128;

    struct ext2_group_desc gd;
    off_t gd_off = (block_size == 1024) ? 2048 : block_size;
    lseek(fd, gd_off, SEEK_SET);
    if (read(fd, &gd, sizeof(gd)) != sizeof(gd)) {
        perror("read group desc");
        return 1;
    }

    // === NEW: Dump block bitmap ===
    off_t bitmap_off = (off_t)gd.bg_block_bitmap * block_size;
    uint8_t *bitmap = malloc(block_size);
    if (!bitmap) { perror("malloc"); return 1; }
    lseek(fd, bitmap_off, SEEK_SET);
    if (read(fd, bitmap, block_size) != (ssize_t)block_size) {
        perror("read block bitmap");
        free(bitmap);
        return 1;
    }

    printf("\n--- Block Bitmap (group 0) ---\n");
    for (uint32_t i = 0; i < sb.s_blocks_per_group; i++) {
        uint32_t byte_index = i / 8;
        uint8_t bit_index = i % 8;
        int allocated = (bitmap[byte_index] >> bit_index) & 1;
        printf("Block %5u: %s\n", i + sb.s_first_data_block, allocated ? "USED" : "FREE");
    }
    free(bitmap);
    printf("--- End of Bitmap ---\n\n");

    // Locate inode table and inode 2
    off_t inode_table = (off_t)gd.bg_inode_table * block_size;
    off_t inode2_off = inode_table + (2 - 1) * inode_size;
    struct ext2_inode inode2;
    lseek(fd, inode2_off, SEEK_SET);
    if (read(fd, &inode2, sizeof(inode2)) != sizeof(inode2)) {
        perror("read inode2");
        return 1;
    }

    printf("Inode 2 size=%u bytes, first block=%u\n",
           inode2.i_size, inode2.i_block[0]);

    uint32_t bytes_remaining = inode2.i_size;
    for (int i = 0; i < 12 && inode2.i_block[i] && bytes_remaining > 0; i++) {
        char *block = malloc(block_size);
        lseek(fd, (off_t)inode2.i_block[i] * block_size, SEEK_SET);
        read(fd, block, block_size);

        uint32_t offset = 0;
        while (offset < block_size && bytes_remaining > 0) {
            struct ext2_dir_entry_2 *de = (struct ext2_dir_entry_2 *)(block + offset);
            if (!de->inode || de->rec_len == 0) break;
            printf("inode=%u rec_len=%u name_len=%u type=%u name=%.*s and offset: %d\n",
                   de->inode, de->rec_len, de->name_len, de->file_type,
                   de->name_len, de->name, offset);
            offset += de->rec_len;
            bytes_remaining -= de->rec_len;
        }
        free(block);
    }

    close(fd);
    return 0;
}
