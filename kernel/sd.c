#include "sd.h"
#include "emmc.h"
#include "defs.h"
#include "list.h"
#include "riscv.h"
#include "spinlock.h"
#include "buf.h"
#include "types.h"
#include "printf.h"
#include "include/fat32.h"
#include "include/ext2.h"
#include "file.h"

static struct emmc sd0;
static struct list_head sdque;
static struct spinlock sdlock;
struct partition_info ptinfo[PARTITIONS];

struct bpb bpb;


// 使用中のパーティション数
static int ptnum = 0;


static inline uint8_t read_le8(const uint8_t *p) {
    return *p;
}

static inline uint16_t read_le16(const uint8_t *p) {
    return p[0] | (p[1] << 8);
}

static inline uint32_t read_le32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

// Hack the partition.
//static uint32_t first_bno = 0;
//static uint32_t nblocks = 1;

// static void sd_sleep(void *chan)
// {
//     sleep(chan, &sdlock);
// }

/*
 * Initialize SD card and parse MBR.
 * 1. The first partition should be FAT and is used for booting.
 * 2. The second partition is used by our file system.
 *
 * See https://en.wikipedia.org/wiki/Master_boot_record
 */
void
sd_init(void)
{
    struct mbr mbr;
    char buf[1024];

    list_init(&sdque);
    initlock(&sdlock, "sd");

    acquire(&sdlock);
    int ret = emmc_init(&sd0);
    //assert(ret == 0);
    if (ret)
      panic("failed emmc_init\n");
    /* lambt9: Set offset to first sector of SD card */
    emmc_seek(&sd0, 0UL);
    size_t bytes = emmc_read(&sd0, buf, 1024);
    if (bytes != 1024)
      panic("failed emmc_read\n");
    //assert(bytes == 512);
    release(&sdlock);

    //char *addr = buf;
    uint32_t byte = 0;
    printf("\n");
    for (int i=0; i < 64; i++) {
      for (int j=0; j < 16; j++) {
        if (j == 0)
          printf("%08x:", byte);
        if (j%2)
          printf("%02x", buf[i*16+j]);
        else
          printf(" %02x", buf[i*16+j]);
      }
      printf("\n");
      byte += 16;
    }

    //assert(mbr.signature == 0xAA55);

    memmove(&mbr, buf, 512);
    for (int i = 0; i < PARTITIONS; i++) {
        if (mbr.ptables[i].lba == 0) break;

        ptinfo[i].type = mbr.ptables[i].type;
        ptinfo[i].lba = mbr.ptables[i].lba;
        ptinfo[i].nsecs = mbr.ptables[i].nsecs;
        info("partition[%d]: TYPE: %d, LBA = 0x%x, #SECS = 0x%x and mbr signature = 0x%x",
            i, ptinfo[i].type, ptinfo[i].lba, ptinfo[i].nsecs, mbr.signature);
        ptnum++;
    }

    uint8_t *volumeID = (uint8_t *)(buf + 512);
    bpb.byts_per_sec = read_le16(volumeID + BPB_BYTSPERSEC_OFFSET);
    bpb.sec_per_clus = read_le8(volumeID + BPB_SECPERCLUS_OFFSET);
    bpb.rsvd_sec_cnt = read_le16(volumeID + BPB_RSVDSECCNT_OFFSET);
    bpb.fat_cnt = read_le8(volumeID + BPB_NUMFATS_OFFSET);

    uint16_t signature = read_le16(volumeID + BPB_SIGNATURE_OFFSET);

    info("VolumeID info: byts_per_sec: %d, sec_per_clus: %d, rsvd_sec_cnt: %d, fat_cnt: %d and signature: %d",
    (int)bpb.byts_per_sec, (int)bpb.sec_per_clus, (int)bpb.rsvd_sec_cnt, (int)bpb.fat_cnt, (int)signature);

#if 0
    acquire(&sdlock);
    emmc_seek(&sd0, 512UL * (ptinfo[1].lba + 1024));
    bytes = emmc_read(&sd0, buf, 512);
    if (bytes != 512)
      panic("failed emmc_read\n");
    //assert(bytes == 512);
    release(&sdlock);

    uint32_t byte = 0;
    printf("\n");
    for (int i=0; i < 32; i++) {
      for (int j=0; j < 16; j++) {
        if (j == 0)
          printf("%08x:", byte);
        if (j%2)
          printf("%02x", buf[i*16+j]);
        else
          printf(" %02x", buf[i*16+j]);
      }
      printf("\n");
      byte += 16;
    }
#endif

    /* Create devsw mapping function for ext2 */
    // devsw[SDCARD].read  = sd_read_ext2;

    /* Find first block of EXT2 partition by Logical Block Address (LBA) of MBR */
    uint32_t ext2_offset = ptinfo[1].lba * SECTOR_SIZE;
    /* First 2 SECTOR of ext2 is for boot sector */
    acquire(&sdlock);
    emmc_seek(&sd0, (uint64_t)(ext2_offset + 2 * SECTOR_SIZE));
    /* Read first 1024 bytes which contain ext2 Superblock after passthrough BOOT RECORD */
    memset(buf, 0, 1024);
    bytes = emmc_read(&sd0, buf, 1024);
    if (bytes != 1024)
      panic("failed emmc_read\n");
    release(&sdlock);

    struct ext2_superblock *sb = (struct ext2_superblock*) buf;
    if (sb->s_magic != 0xEF53) {
      panic("Not an ext2 partition");
    }
    /* Now our ext2 FS has size = 32M
    Each block ~ 4096 bytes
    ==> 32M/4096 = 8192 blocks
    - One block group has 32768 blocks
    --> So we have only one block group
    */
    printf("Ext2 block count %d\n", sb->s_blocks_count);
    printf("Ext2 inode count %d\n", sb->s_inodes_count);
    /* Print block group of superblock ==> 0 is correct */
    printf("Ext2 number of block groups %d\n", sb->s_block_group_nr);
    printf("Ext2 block groups count: %d\n", (int)(sb->s_blocks_count / sb->s_blocks_per_group) + 1);
    printf("Ext2 block size: %d\n", 1024 << sb->s_log_block_size);
    printf("Ext2 block/group: %d and inodes/group: %d\n",
           sb->s_blocks_per_group, sb->s_inodes_per_group);
    printf("First data block: %d\n", sb->s_first_data_block);


    /* Try to extract some ext2 superblock information */
    struct ext2_sb_info sbi;
    
    /* Number of block group descriptor/block */
    sbi.s_desc_per_block = (1024 << sb->s_log_block_size) / sizeof(struct ext2_group_desc);

    sbi.s_blocks_per_group = sb->s_blocks_per_group;
    sbi.s_groups_count = ((sb->s_blocks_count -
                          sb->s_first_data_block - 1)
                            / sbi.s_blocks_per_group) + 1;

    int db_count = (sbi.s_groups_count + sbi.s_desc_per_block - 1) /
              sbi.s_desc_per_block;

    printf("Ext2 block group descriptor count: %d and descriptor block count: %d\n", sbi.s_groups_count, db_count);
    /* Try to dump all block group descriptor */
    /* After superblock is block descriptor list*/

    acquire(&sdlock);
    emmc_seek(&sd0, (uint64_t)(ext2_offset + 2 * SECTOR_SIZE + 1024));
    /* Read first 1024 bytes which contain ext2 Superblock after passthrough BOOT RECORD */
    memset(buf, 0, 1024);
    bytes = emmc_read(&sd0, buf, 1024 * db_count);
    if (bytes != 1024)
      panic("failed emmc_read\n");
    release(&sdlock);


    struct ext2_group_desc *gd = (struct ext2_group_desc *)buf;
    // Example: dump block group descriptors (replace N with actual count if needed)
    for (int i = 0; i < sbi.s_groups_count; i++) {
        // You can print or process gd[i] here
        printf("Group %d: block_bitmap = %u, inode_bitmap = %u, inode_table = %u and num_dir = %u\n",
               i, gd[i].bg_block_bitmap, gd[i].bg_inode_bitmap, gd[i].bg_inode_table, gd[i].bg_used_dirs_count);
    }

    /* Now try to read content of a file from first block group*/
    uint64_t bg_inode_bitmap_offset = gd[0].bg_inode_bitmap * 1024;
    uint64_t bg_block_bitmap_offset = gd[0].bg_block_bitmap * 1024;
    uint64_t bg_inode_table_offset = gd[0].bg_inode_table * 1024;

    acquire(&sdlock);
    emmc_seek(&sd0, (uint64_t)(ext2_offset + bg_inode_bitmap_offset));
    /* Read the inode bitmap */
    char inode_bitmap[1024];
    memset(inode_bitmap, 0, 1024);
    bytes = emmc_read(&sd0, inode_bitmap, 1024);
    if (bytes != 1024)
      error("failed emmc_read inode_bitmap\n");

    emmc_seek(&sd0, (uint64_t)(ext2_offset + bg_block_bitmap_offset));
    /* Read the block bitmap */
    char block_bitmap[1024];
    memset(block_bitmap, 0, 1024);    
    bytes = emmc_read(&sd0, block_bitmap, 1024);
    if (bytes != 1024)
      error("failed emmc_read block_bitmap\n"); 

    emmc_seek(&sd0, (uint64_t)(ext2_offset + bg_inode_table_offset));
    /* Read the inode table */
    char inode_table[1024 * 8]; /* Assume inode table fits in 8 blocks */
    memset(inode_table, 0, 1024 * 8);    
    bytes = emmc_read(&sd0, inode_table, 1024 * 8);
    if (bytes != 1024 * 8)
      error("failed emmc_read inode_table\n"); 
    release(&sdlock);

    /* Inode address start at 1 so find block group contains inode:
    block_group = (ino - 1) / inodes_per_group
    index = (ino - 1) % inodes_per_group
    block = index / inodes_per_block
    offset = index % inodes_per_block * inode_size
    */

    uint64_t inode_per_group = sb->s_inodes_per_group;
    uint16_t inode_size = sb->s_inode_size;
    inode_size = inode_size ? inode_size : 128; /* Default inode size */
    printf("Inode per group: %d and inode size: %d\n", (int)inode_per_group, (int)inode_size);

    printf("Ext2 revision level: %d\n", sb->s_rev_level);
    printf("First non-reserved inode: %d\n", sb->s_first_ino);
    printf("Root inode is %d\n", EXT2_ROOT_INO);

    /* Try to read root inode is number 2 so we calculate root_inode_offset in inode table */
    int root_inode_offset = (EXT2_ROOT_INO - 1) * inode_size;
    struct ext2_inode *root_inode = (struct ext2_inode *)(inode_table + root_inode_offset);
    printf("Root inode: mode = %x, size = %u, blocks = %u\n",
           root_inode->i_mode, root_inode->i_size, root_inode->i_blocks);
    printf("Root inode block pointers: ");
    for (int i = 0; i < EXT2_N_BLOCKS; i++) {
        printf("%u ", root_inode->i_block[i]);
    }
    printf("\n");   
    /* Now try to read root directory entries */
    if ((root_inode->i_mode & 0xF000) != 0x4000) {
      panic("Root inode is not a directory");
    }   

    uint32_t dir_block = root_inode->i_block[0];
    uint64_t dir_block_offset = dir_block * 1024;   
    acquire(&sdlock);
    emmc_seek(&sd0, (uint64_t)(ext2_offset + dir_block_offset));
    /* Read the directory block */
    char dir_block_data[1024];
    memset(dir_block_data, 0, 1024);    
    bytes = emmc_read(&sd0, dir_block_data, 1024);
    if (bytes != 1024)
      error("failed emmc_read dir_block_data\n"); 
    release(&sdlock);
    /* Parse directory entries */
    uint32_t offset = 0;
    printf("Root directory entries:\n");
    while (offset < 1024) {
        struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(dir_block_data + offset);
        if (entry->inode == 0) break;
        char name[256];
        memcpy(name, entry->name, entry->name_len);
        name[entry->name_len] = '\0';
        printf("Inode: %u, Name: %s, Rec_len: %u, Name_len: %u\n",
               entry->inode, name, entry->rec_len, entry->name_len);
        offset += entry->rec_len;
        if (strncmp(name, "test.txt", strlen(name)) == 0) {
          /* Found test.txt, read its inode */
          uint64_t file_inode_offset = (entry->inode - 1) * inode_size;
          struct ext2_inode *file_inode = (struct ext2_inode *)(inode_table + file_inode_offset);
          printf("File inode: mode = %x, size = %u, blocks = %u\n",
                 file_inode->i_mode, file_inode->i_size, file_inode->i_blocks);
          printf("File inode block pointers: ");
          for (int i = 0; i < EXT2_N_BLOCKS; i++) {
              printf("%u ", file_inode->i_block[i]);
          }
          printf("\n");
          if ((file_inode->i_mode & 0xF000) != 0x8000) {
            panic("test.txt is not a regular file");
          }
          /* Read first data block of hello.txt */
          uint32_t file_block = file_inode->i_block[0];
          uint64_t file_block_offset = file_block * 1024;
          acquire(&sdlock);
          emmc_seek(&sd0, (uint64_t)(ext2_offset + file_block_offset));
          char file_data[1024];
          memset(file_data, 0, 1024);
          bytes = emmc_read(&sd0, file_data, 1024);
          if (bytes != 1024)
            error("failed emmc_read file_data\n");
          release(&sdlock);
          printf("Content of hello.txt:\n%s\n", file_data);
        }
    }
    info("sd_init ok\n");
}

int sd_read_ext2(int dev, uint32_t blockno, char *buf)
{
    if (dev != SDCARD)
        return -1;

    //uint32_t sector_per_block = 1024 / SD_BLOCK_SIZE;

    /* Calculate ext2 offset must based on SECTOR size of disk */
    uint32_t ext2_offset = ptinfo[1].lba * SECTOR_SIZE;
    acquire(&sdlock);
    emmc_seek(&sd0, (uint64_t)(ext2_offset + blockno * EXT2_DEFAULT_BLOCK_SIZE));
    size_t bytes = emmc_read(&sd0, buf, EXT2_DEFAULT_BLOCK_SIZE);
    release(&sdlock);
    if (bytes != EXT2_DEFAULT_BLOCK_SIZE) {
      error("sd_read_ext2 failed\n");
      return -1;  
    }
    return 0;
}

void
sd_intr(void)
{
    acquire(&sdlock);
    emmc_intr(&sd0);
    //disb();
    wakeup(&sd0);
    release(&sdlock);
}


/*
 * SDカードのリクエスト処理を開始する.
 * Callerはsdlockを保持していなければならない.
 */
#if 0
void sd_start(void)
{
    //uint32_t bno;

    while (!list_empty(&sdque)) {
        struct buf *b =
            container_of(list_front(&sdque), struct buf, dlink);
        // TODO: block sizeをvfsで持つ
        uint32_t blks = b->dev == FATMINOR ? 512 : 1024;
        trace("buf blockno: 0x%08x, flags: 0x%08x", b->blockno, b->flags);
        emmc_seek(&sd0, b->blockno * SD_BLOCK_SIZE);

        if (b->flags & B_DIRTY) {
            assert(emmc_write(&sd0, b->data, blks) == blks);
        } else {
            assert(emmc_read(&sd0, b->data, blks) == blks);
        }

        b->flags |= B_VALID;
        b->flags &= ~B_DIRTY;

        list_pop_front(&sdque);

        //disb();
        wakeup(b);
    }
}

void sd_rw(struct buf *b)
{
    acquire(&sdlock);

    // Append to request queue.
    list_push_back(&sdque, &b->dlink);

    // Start disk if necessary.
    if (list_front(&sdque) == &b->dlink) {
        sd_start();
    }

    // Wait for request to finish.
    while ((b->flags & (B_VALID | B_DIRTY)) != B_VALID)
        sd_sleep(b);

    release(&sdlock);
}
#endif
