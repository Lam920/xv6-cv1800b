#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREATE  0x200
#define O_TRUNC   0x400

#ifdef LAB_MMAP
#define PROT_NONE   (0L << 0)
#define PROT_READ   (1L << 1)
#define PROT_WRITE  (1L << 2)
#define PROT_EXEC   (1L << 4)

#define MAP_SHARED      0x01
#define MAP_PRIVATE     0x02
#endif