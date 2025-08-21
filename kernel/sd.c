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
    printf("Ext2 block count %d\n", sb->s_blocks_count);
    printf("Ext2 inode count %d\n", sb->s_inodes_count);
    printf("Ext2 number of block groups %d\n", sb->s_block_group_nr);
    printf("Ext2 blocks count: %d\n", (int)(sb->s_blocks_count / sb->s_blocks_per_group) + 1);
    printf("Ext2 block size: %d\n", 1024 << sb->s_log_block_size);
    printf("Ext2 block/group: %d and inodes/group: %d\n",
           sb->s_blocks_per_group, sb->s_inodes_per_group);
    printf("First data block: %d\n", sb->s_first_data_block);

    info("sd_init ok\n");
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
