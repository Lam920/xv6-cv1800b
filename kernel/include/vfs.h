// Simple VFS implementation

#include "../types.h"
#include "../param.h"
#include "../list.h"
#include "../stat.h"
#include "../spinlock.h"

#ifndef XV6_VFS_H_
#define XV6_VFS_H_

struct buf;

#define min(a, b) ((a) < (b) ? (a) : (b))

struct inode_operations {
  struct inode* (*dirlookup)(struct inode *dp, char *name, uint *off);
  void (*iupdate)(struct inode *ip);
  void (*itrunc)(struct inode *ip);
  void (*cleanup)(struct inode *ip);
  uint (*bmap)(struct inode *ip, uint bn);
  void (*ilock)(struct inode* ip);
  void (*iunlock)(struct inode* ip);
  void (*stati)(struct inode *ip, struct stat *st);
  int (*readi)(struct inode *ip, int user_dst, uint64 dst, uint off, uint n);
  int (*writei)(struct inode *ip, int user_src, uint64 src, uint off, uint n);
  int (*dirlink)(struct inode *dp, char *name, uint inum, uint type);
  int (*unlink)(struct inode *dp, uint off);
  int (*isdirempty)(struct inode *dp);
  int (*dummy)(void); // To avoid empty struct
};

// in-memory copy of an inode
struct inode {
  uint dev;                     // Minor Device number ~ lambt9: Where this inode is stored on
  uint inum;                    // Inode number
  int ref;                      // Reference count ~ lambt9 = 0 ==> Can reuse this node for another dinode
  int flags;                    // I_BUSY, I_VALID ~ lambt9: use by the VFS algorithms
//   struct sleeplock lock;        // protects everything below here
  struct filesystem_type *fs_t; // The Filesystem type this inode is stored in
  struct inode_operations *iops; // The specific inode operations ~ lambt9 ~ fs_t->iops
  void *i_private;               // File System specific informations ~ lambt9: dinode specific information
  // ~ example: addrs of block on disk

  short type;           // File type ~ lambt9: T_DIR, T_FILE, T_DEV, T_MOUNT
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
};

struct superblock {
  int major;        // Driver major number from it superblocks is stored in.
  int minor;        // Driver major number from it superblocks is stored in.
  uint blocksize;  // Block size of this superblock
  void *fs_info;    // Filesystem-specific info
  unsigned char s_blocksize_bits;

  int flags;       // Superblock Flags to map its usage
};

#define SB_NOT_LOADED 0
#define SB_INITIALIZED 1

#define SB_FREE 0
#define SB_USED 1



#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)



#define INODE_FREE 0
#define INODE_USED 1

#define I_BUSY 0x1
#define I_VALID 0x2

// Inode main operations
struct inode* iget(uint dev, uint inum, int (*fill_super)(struct inode *));

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

struct vfs_operations {
  int           (*fs_init)(void);
  int           (*mount)(struct inode *, struct inode *);
  int           (*unmount)(struct inode *);
  struct inode* (*getroot)(int, int);
  void          (*readsb)(int dev, struct superblock *sb);
  struct inode* (*ialloc)(uint dev, short type);
  uint          (*balloc)(uint dev);
  void          (*bzero)(int dev, int bno);
  void          (*bfree)(int dev, uint b);
  void          (*brelse)(struct buf *b);
  void          (*bwrite)(struct buf *b);
  struct buf*   (*bread)(uint dev, uint blockno);
  int           (*namecmp)(const char *s, const char *t);
  void          (*dummy)(void); // To avoid empty struct
};

/*
 * This is struct is the map block device and its filesystem.
 * Its main job is return the filesystem type of current (major, minor)
 * mounted device. It is used when it is not possible retrive the
 * filesystem_type from the inode.
 */
struct vfs {
  int major;
  int minor;
  int flag;
  struct filesystem_type *fs_t;
  struct list_head fs_next; // Next mounted on vfs
};
#define VFS_FREE 0
#define VFS_USED 1


struct itable {
  struct spinlock lock;
  struct inode inode[NINODE];
};

struct vfsmlist {
  struct spinlock lock;
  struct list_head fs_list;
};

extern struct vfs *rootfs; // It is the global pointer to root fs entry

extern struct itable itable;
/*
 * This is te representation of mounted lists.
 * It is defferent from the vfssw, because it is mapping the mounted
 * on filesystem per (major, minor)
 */
extern struct vfsmlist vfsmlist;

struct filesystem_type {
  char *name;                     // The filesystem name. Its is used by the mount syscall
  struct vfs_operations *ops;     // VFS operations
  struct inode_operations *iops;  // Pointer to inode operations of this FS.
  struct list_head fs_list;       // This is a list of Filesystems used by vfssw
};

void            installrootfs(void);
void            initvfsmlist(void);
struct vfs*     getvfsentry(int major, int minor);
int             putvfsonlist(int major, int minor, struct filesystem_type *fs_t);
void            initvfssw(void);
int             register_fs(struct filesystem_type *fs);
struct filesystem_type* getfs(const char *fs_name);

// Generic inode operations
void generic_iunlock(struct inode*);
void generic_stati(struct inode *ip, struct stat *st);
int  generic_readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n);
int  generic_dirlink(struct inode *dp, char *name, uint inum, uint type);

int sb_set_blocksize(struct superblock *sb, int size);

#endif /* XV6_VFS_H_ */

