/**
 * This is the ext2 implementation
 * It is based on Linux ext2 implementation
 **/

#include "../types.h"

#ifndef XV6_EXT2_H_
#define XV6_EXT2_H_

/* data type for block offset of block group */
typedef int ext2_grpblk_t;

/* data type for filesystem-wide blocks number */
typedef unsigned long ext2_fsblk_t;

#define EXT2_MIN_BLKSIZE 1024
#define EXT2_SUPER_MAGIC 0xEF53

#define EXT2_MAX_BGC 40

#define EXT2_NAME_LEN 255

/**
 * EXT2_DIR_PAD defines the directory entries boundaries
 *
 * NOTE: It must be a multiple of 4
 */
#define EXT2_DIR_PAD                    4
#define EXT2_DIR_ROUND                  (EXT2_DIR_PAD - 1)
#define EXT2_DIR_REC_LEN(name_len)      (((name_len) + 8 + EXT2_DIR_ROUND) & \
                                         ~EXT2_DIR_ROUND)
#define EXT2_MAX_REC_LEN                ((1<<16)-1)

/**
 * Ext2 directory file types.  Only the low 3 bits are used.  The
 * other bits are reserved for now.
 */
enum {
  EXT2_FT_UNKNOWN  = 0,
  EXT2_FT_REG_FILE = 1,
  EXT2_FT_DIR      = 2,
  EXT2_FT_CHRDEV   = 3,
  EXT2_FT_BLKDEV   = 4,
  EXT2_FT_FIFO     = 5,
  EXT2_FT_SOCK     = 6,
  EXT2_FT_SYMLINK  = 7,
  EXT2_FT_MAX
};
  
#define EXT2_S_IFREG 0x8000
#define EXT2_S_IFDIR 0x4000

// Stat operations

#define S_IFMT  00170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

#define S_ISLNK(m)     (((m) & S_IFMT) == S_IFLNK)
#define S_ISREG(m)     (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)     (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)     (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)     (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m)    (((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m)    (((m) & S_IFMT) == S_IFSOCK)

#define S_IR  WXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100

#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010

#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001


/**
 * This struct is based on the Linux Sorce Code fs/ext2/ext2.h.
 * It is the ext2 superblock layout definition.
 */
struct ext2_superblock {
  uint32 s_inodes_count;    /* Inodes count */
  uint32 s_blocks_count;    /* Blocks count */
  uint32 s_r_blocks_count;  /* Reserved blocks count */
  uint32 s_free_blocks_count;  /* Free blocks count */
  uint32 s_free_inodes_count;  /* Free inodes count */
  uint32 s_first_data_block;  /* First Data Block */
  uint32 s_log_block_size;  /* Block size */
  uint32 s_log_frag_size;  /* Fragment size */
  uint32 s_blocks_per_group;  /* # Blocks per group */
  uint32 s_frags_per_group;  /* # Fragments per group */
  uint32 s_inodes_per_group;  /* # Inodes per group */
  uint32 s_mtime;    /* Mount time */
  uint32 s_wtime;    /* Write time */
  uint16 s_mnt_count;    /* Mount count */
  uint16 s_max_mnt_count;  /* Maximal mount count */
  uint16 s_magic;    /* Magic signature */
  uint16 s_state;    /* File system state */
  uint16 s_errors;    /* Behaviour when detecting errors */
  uint16 s_minor_rev_level;   /* minor revision level */
  uint32 s_lastcheck;    /* time of last check */
  uint32 s_checkinterval;  /* max. time between checks */
  uint32 s_creator_os;    /* OS */
  uint32 s_rev_level;    /* Revision level */
  uint16 s_def_resuid;    /* Default uid for reserved blocks */
  uint16 s_def_resgid;    /* Default gid for reserved blocks */

  /*
   * These fields are for EXT2_DYNAMIC_REV superblocks only.
   *
   * Note: the difference between the compatible feature set and
   * the incompatible feature set is that if there is a bit set
   * in the incompatible feature set that the kernel doesn't
   * know about, it should refuse to mount the filesystem.
   *
   * e2fsck's requirements are more strict; if it doesn't know
   * about a feature in either the compatible or incompatible
   * feature set, it must abort and not try to meddle with
   * things it doesn't understand...
   */
  uint32 s_first_ino;     /* First non-reserved inode */
  uint16 s_inode_size;     /* size of inode structure */
  uint16 s_block_group_nr;   /* block group # of this superblock */
  uint32 s_feature_compat;   /* compatible feature set */
  uint32 s_feature_incompat;   /* incompatible feature set */
  uint32 s_feature_ro_compat;   /* readonly-compatible feature set */
  uint8  s_uuid[16];    /* 128-bit uuid for volume */
  char   s_volume_name[16];   /* volume name */
  char   s_last_mounted[64];   /* directory where last mounted */
  uint32 s_algorithm_usage_bitmap; /* For compression */

  /*
   * Performance hints.  Directory preallocation should only
   * happen if the EXT2_COMPAT_PREALLOC flag is on.
   */
  uint8  s_prealloc_blocks;  /* Nr of blocks to try to preallocate*/
  uint8  s_prealloc_dir_blocks;  /* Nr to preallocate for dirs */
  uint16 s_padding1;

  /*
   * Journaling support valid if EXT3_FEATURE_COMPAT_HAS_JOURNAL set.
   */
  uint8  s_journal_uuid[16];  /* uuid of journal superblock */
  uint32 s_journal_inum;    /* inode number of journal file */
  uint32 s_journal_dev;    /* device number of journal file */
  uint32 s_last_orphan;    /* start of list of inodes to delete */
  uint32 s_hash_seed[4];    /* HTREE hash seed */
  uint8  s_def_hash_version;  /* Default hash version to use */
  uint8  s_reserved_char_pad;
  uint16 s_reserved_word_pad;
  uint32 s_default_mount_opts;
  uint32 s_first_meta_bg;   /* First metablock block group */
  uint32 s_reserved[190];  /* Padding to the end of the block */
};

#endif /* XV6_EXT2_h */

