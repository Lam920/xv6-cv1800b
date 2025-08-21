#ifndef FAT32_H
#define FAT32_H

#include "../types.h"

#define BPB_BYTSPERSEC_OFFSET   0x0B
#define BPB_SECPERCLUS_OFFSET   0x0D
#define BPB_RSVDSECCNT_OFFSET   0x0E
#define BPB_NUMFATS_OFFSET      0x10
#define BPB_FATSZ32_OFFSET      0x24
#define BPB_ROOTCLUS_OFFSET     0x2C
#define BPB_SIGNATURE_OFFSET    0x1FE

struct bpb {
        uint16_t  byts_per_sec;
        uint8_t   sec_per_clus;
        uint16_t  rsvd_sec_cnt;
        uint8_t   fat_cnt;            /* count of FAT regions */
        uint32_t  hidd_sec;           /* count of hidden sectors */
        uint32_t  tot_sec;            /* total count of sectors including all regions */
        uint32_t  fat_sz;             /* count of sectors for a FAT region */
        uint32_t  root_clus;
};

#endif