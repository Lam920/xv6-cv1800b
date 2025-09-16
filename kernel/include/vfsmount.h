// This file implements the Mount table and utilities functions
#include "vfs.h"
#include "../param.h"
#include "../file.h"
#include "../spinlock.h"

#ifndef XV6_VFSMOUNT_H_
#define XV6_VFSMOUNT_H_

#define M_USED 0x1
#define MOUNTSIZE     2   // size of mounted devices

// Mount Table Entry
struct mntentry {
  struct inode *m_inode; // lambt9: inode which FS is mounted
  struct inode *m_rtinode; // Root inode for device ~ lambt9: inode of the root of  mounted FS
  void *pdata;             // Private date of mountentry. Almost is a superblock
  int dev;                 // Mounted device
  int flag;                // Flag
};

// Mount Table Structure
struct mtable{
  struct spinlock lock;
  struct mntentry mpoint[MOUNTSIZE];
};

extern struct mtable mtable; // The global mount table

// Utility functions

struct inode* mtablertinode(struct inode * ip); // lambt9: return the root inode of the mounted FS
struct inode* mtablemntinode(struct inode * ip); // lambt9: return mount point
int isinoderoot(struct inode* ip);
void mountinit(void);

#endif /* XV6_VFSMOUNT_H_ */

