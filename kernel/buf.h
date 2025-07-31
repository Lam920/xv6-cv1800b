#ifndef INC_BUF_H
#define INC_BUF_H

#include "types.h"
#include "sleeplock.h"

#define DSIZE 1024

struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[DSIZE];
};


#endif
