#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ext2fs/ext2_fs.h>

#define BLOCK_SIZE 1024

void read_block(int fd, unsigned long block_no, void *buffer) {
    lseek(fd, block_no * BLOCK_SIZE, SEEK_SET);
    read(fd, buffer, BLOCK_SIZE);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <ext2 device>\n", argv[0]);
        fprintf(stderr, "Example: %s /dev/sdb2\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    // Read superblock
    struct ext2_super_block sb;
    read_block(fd, 1, &sb);  // Superblock is at block 1

    if (sb.s_magic != EXT2_SUPER_MAGIC) {
        fprintf(stderr, "Not an ext2 filesystem\n");
        close(fd);
        return 1;
    }

    printf("Ext2 filesystem detected:\n");
    printf("Block size: %u\n", BLOCK_SIZE);
    printf("Inodes per group: %u\n", sb.s_inodes_per_group);
    printf("Blocks per group: %u\n", sb.s_blocks_per_group);
    printf("First data block: %u\n", sb.s_first_data_block);
    printf("Inode size: %u\n", sb.s_inode_size);
    printf("\n");

    // Read group descriptor 0
    struct ext2_group_desc gd;
    unsigned long gd_block = sb.s_first_data_block + 1;  // Group descriptors after superblock
    lseek(fd, gd_block * BLOCK_SIZE, SEEK_SET);
    read(fd, &gd, sizeof(gd));

    printf("Group descriptor 0:\n");
    printf("Inode table block: %u\n", gd.bg_inode_table);
    printf("Root directory inode: %u\n", EXT2_ROOT_INO);
    printf("\n");

    // Read root inode (inode 2)
    struct ext2_inode root_inode;
    unsigned long inode_table_block = gd.bg_inode_table;
    unsigned long inode_offset = (EXT2_ROOT_INO - 1) % sb.s_inodes_per_group;
    unsigned long inode_block = inode_table_block + (inode_offset * sb.s_inode_size) / BLOCK_SIZE;
    unsigned long inode_block_offset = (inode_offset * sb.s_inode_size) % BLOCK_SIZE;

    lseek(fd, inode_block * BLOCK_SIZE + inode_block_offset, SEEK_SET);
    read(fd, &root_inode, sizeof(root_inode));

    printf("Root inode (inode 2) info:\n");
    printf("Mode: %o\n", root_inode.i_mode);
    printf("Size: %u\n", root_inode.i_size);
    printf("Blocks: %u\n", root_inode.i_blocks);
    printf("Direct blocks: ");
    for (int i = 0; i < 12; i++) {
        if (root_inode.i_block[i])
            printf("%u ", root_inode.i_block[i]);
    }
    printf("\n\n");

    // Read root directory blocks
    printf("Root directory entries:\n");
    printf("%-20s %-10s %-10s %s\n", "Filename", "Inode", "RecLen", "Type");
    printf("-------------------------------------------------\n");

    for (int i = 0; i < 12; i++) {
        if (root_inode.i_block[i] == 0) break;

        char block_buffer[BLOCK_SIZE];
        read_block(fd, root_inode.i_block[i], block_buffer);

        struct ext2_dir_entry_2 *dir_entry = (struct ext2_dir_entry_2 *)block_buffer;
        char *limit = block_buffer + BLOCK_SIZE;

        while ((char *)dir_entry < limit) {
            if (dir_entry->inode == 0) break;
            if (dir_entry->name_len == 0) break;

            char filename[256];
            memcpy(filename, dir_entry->name, dir_entry->name_len);
            filename[dir_entry->name_len] = '\0';

            char filetype = '?';
            switch (dir_entry->file_type) {
                case EXT2_FT_REG_FILE: filetype = 'F'; break;
                case EXT2_FT_DIR: filetype = 'D'; break;
                case EXT2_FT_SYMLINK: filetype = 'L'; break;
                default: filetype = '?';
            }

            printf("%-20s %-10u %-10u %c\n", 
                   filename, dir_entry->inode, dir_entry->rec_len, filetype);

            if (dir_entry->rec_len == 0) break;
            dir_entry = (struct ext2_dir_entry_2 *)((char *)dir_entry + dir_entry->rec_len);
        }
    }

    close(fd);
    return 0;
}