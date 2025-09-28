#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

// ext2 filesystem constants and structures
#define EXT2_SUPER_MAGIC 0xEF53

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
    
    // Extended fields we need
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algorithm_usage_bitmap;
    
    // Performance hints
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_padding1;
    
    // Journaling support
    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    
    // Directory indexing support
    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_padding2[3];
    
    // Other options
    uint32_t s_default_mount_options;
    uint32_t s_first_meta_bg;
    uint8_t  s_padding3[760];
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

// Function to read superblock
int read_superblock(int fd, struct ext2_super_block *sb) {
    if (lseek(fd, 1024, SEEK_SET) == -1) {
        perror("lseek superblock");
        return -1;
    }
    
    if (read(fd, sb, sizeof(struct ext2_super_block)) != sizeof(struct ext2_super_block)) {
        perror("read superblock");
        return -1;
    }
    
    if (sb->s_magic != EXT2_SUPER_MAGIC) {
        fprintf(stderr, "Not an ext2 filesystem (magic: 0x%x)\n", sb->s_magic);
        return -1;
    }
    
    return 0;
}

// Function to read block group descriptor table
struct ext2_group_desc *read_bgdt(int fd, struct ext2_super_block *sb, int bg_count) {
    int block_size = 1024 << sb->s_log_block_size;
    int bgdt_block = (sb->s_first_data_block == 1) ? 2 : 1;
    
    if (lseek(fd, bgdt_block * block_size, SEEK_SET) == -1) {
        perror("lseek bgdt");
        return NULL;
    }
    
    struct ext2_group_desc *bgdt = malloc(bg_count * sizeof(struct ext2_group_desc));
    if (!bgdt) {
        perror("malloc bgdt");
        return NULL;
    }
    
    if (read(fd, bgdt, bg_count * sizeof(struct ext2_group_desc)) != bg_count * sizeof(struct ext2_group_desc)) {
        perror("read bgdt");
        free(bgdt);
        return NULL;
    }
    
    return bgdt;
}

// Function to dump bitmap
void dump_bitmap(int fd, uint32_t bitmap_block, int block_size, const char *bitmap_name, int group) {
    uint8_t *bitmap = malloc(block_size);
    if (!bitmap) {
        perror("malloc bitmap");
        return;
    }
    
    if (lseek(fd, bitmap_block * block_size, SEEK_SET) == -1) {
        perror("lseek bitmap");
        free(bitmap);
        return;
    }
    
    if (read(fd, bitmap, block_size) != block_size) {
        perror("read bitmap");
        free(bitmap);
        return;
    }
    
    printf("\n%s for Block Group %d:\n", bitmap_name, group);
    printf("Block address: %u (0x%x)\n", bitmap_block, bitmap_block);
    printf("Bitmap contents (%d bytes):\n", block_size);
    
    // Print hex dump
    for (int i = 0; i < block_size; i++) {
        if (i % 16 == 0) {
            if (i > 0) printf("\n");
            printf("%04x: ", i);
        }
        printf("%02x ", bitmap[i]);
    }
    printf("\n");
    
    // Print binary representation (first 256 bits to avoid too much output)
    printf("First 256 bits (LSB first): ");
    int max_bits = (block_size * 8 < 256) ? block_size * 8 : 256;
    for (int i = 0; i < max_bits; i++) {
        int byte_index = i / 8;
        int bit_index = i % 8;
        int bit = (bitmap[byte_index] >> bit_index) & 1;
        printf("%d", bit);
        if ((i + 1) % 64 == 0 && i < max_bits - 1) {
            printf("\n");
        }
    }
    printf("\n");
    
    // Count set bits
    int set_bits = 0;
    for (int i = 0; i < block_size; i++) {
        for (int j = 0; j < 8; j++) {
            if (bitmap[i] & (1 << j)) {
                set_bits++;
            }
        }
    }
    printf("Total bits set: %d/%d\n", set_bits, block_size * 8);
    
    free(bitmap);
}

int main(int argc, char *argv[]) {
    const char *device = "/dev/sdc2";
    int fd;
    struct ext2_super_block sb;
    struct ext2_group_desc *bgdt = NULL;
    
    if (argc > 1) {
        device = argv[1];
    }
    
    printf("Opening device: %s\n", device);
    
    // Open device
    fd = open(device, O_RDONLY);
    if (fd == -1) {
        perror("open device");
        return 1;
    }
    
    // Read superblock
    if (read_superblock(fd, &sb) == -1) {
        close(fd);
        return 1;
    }
    
    printf("EXT2 Filesystem Information:\n");
    printf("Magic: 0x%x\n", sb.s_magic);
    printf("Total blocks: %u\n", sb.s_blocks_count);
    printf("Total inodes: %u\n", sb.s_inodes_count);
    printf("Blocks per group: %u\n", sb.s_blocks_per_group);
    printf("Inodes per group: %u\n", sb.s_inodes_per_group);
    printf("Block size: %d\n", 1024 << sb.s_log_block_size);
    
    // Calculate number of block groups
    int bg_count = (sb.s_blocks_count + sb.s_blocks_per_group - 1) / sb.s_blocks_per_group;
    printf("Number of block groups: %d\n", bg_count);
    
    // Read block group descriptor table
    bgdt = read_bgdt(fd, &sb, bg_count);
    if (!bgdt) {
        close(fd);
        return 1;
    }
    
    int block_size = 1024 << sb.s_log_block_size;
    
    // Dump bitmaps for each block group
    for (int i = 0; i < bg_count; i++) {
        printf("\n========================================\n");
        printf("BLOCK GROUP %d\n", i);
        printf("========================================\n");
        
        printf("Block bitmap block: %u\n", bgdt[i].bg_block_bitmap);
        printf("Inode bitmap block: %u\n", bgdt[i].bg_inode_bitmap);
        printf("Inode table block: %u\n", bgdt[i].bg_inode_table);
        printf("Free blocks count: %u\n", bgdt[i].bg_free_blocks_count);
        printf("Free inodes count: %u\n", bgdt[i].bg_free_inodes_count);
        printf("Used directories count: %u\n", bgdt[i].bg_used_dirs_count);
        
        // Dump block bitmap
        dump_bitmap(fd, bgdt[i].bg_block_bitmap, block_size, "BLOCK BITMAP", i);
        
        // Dump inode bitmap
        dump_bitmap(fd, bgdt[i].bg_inode_bitmap, block_size, "INODE BITMAP", i);
    }
    
    free(bgdt);
    close(fd);
    
    return 0;
}