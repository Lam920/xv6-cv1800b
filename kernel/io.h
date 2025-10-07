/* Copyright (C) 2024 Jisheng Zhang <jszhang@kernel.org> */

#ifndef _IO_H__
#define _IO_H_

#include "types.h"

#define BIT(nr)         (1UL << (nr))
#define MMC_CAP(mode)   (1 << mode)

static inline void write8(unsigned long addr, uint8 value)
{
  *(volatile uint8 *)addr = value;
}

static inline uint8 read8(unsigned long addr)
{
  return *(volatile uint8 *)addr;
}

static inline void write16(unsigned long addr, uint16 value)
{
  *(volatile uint16 *)addr = value;
}

static inline uint16 read16(unsigned long addr)
{
  return *(volatile uint16 *)addr;
}

static inline void write32(unsigned long addr, uint32 value)
{
  *(volatile uint32 *)addr = value;
}

static inline uint32 read32(unsigned long addr)
{
  return *(volatile uint32 *)addr;
}

static inline void write64(unsigned long addr, uint64 value)
{
  *(volatile uint64 *)addr = value;
}

static inline uint64 read64(unsigned long addr)
{
  return *(volatile uint64 *)addr;
}


// addrのレジスタのclearビットをクリアする
static inline void clrbits32(uintptr_t addr, uint32_t clear)
{
    write32(addr, read32(addr) & ~clear);
}

// addrのレジスタのsetビットをセットする
static inline void setbits32(uintptr_t addr, uint32_t set)
{
    write32(addr, read32(addr) | set);
}

// addrのレジスタのclearビットをクリアしてsetビットをセットする
static inline void clrsetbits32(uintptr_t addr, uint32_t clear, uint32_t set)
{
    write32(addr, (read32(addr) & ~clear) | set);
}



#define readb(addr) \
	({ unsigned char __v = (*(volatile unsigned char *)(addr)); __v; })
#define readw(addr) \
	({ unsigned short __v = (*(volatile unsigned short *)(addr)); __v; })
#define readl(addr) \
	({ unsigned int __v = (*(volatile unsigned int *)(addr)); __v; })
#define writeb(b, addr) (void)((*(volatile unsigned char *)(addr)) = (b))
#define writew(b, addr) (void)((*(volatile unsigned short *)(addr)) = (b))
#define writel(b, addr) (void)((*(volatile unsigned int *)(addr)) = (b))


#endif
