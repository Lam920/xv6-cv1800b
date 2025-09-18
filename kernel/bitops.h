/* From U-boot */
#ifndef INC_BITOPS_H
#define INC_BITOPS_H
/*
 * ffs: find first bit set. This is defined the same way as
 * the libc and compiler builtin ffs routines, therefore
 * differs in spirit from the above ffz (man ffs).
 */

 #include "types.h"
 #include "defs.h"

static inline int generic_ffs(int x)
{
	int r = 1;

	if (!x)
		return 0;
	if (!(x & 0xffff)) {
		x >>= 16;
		r += 16;
	}
	if (!(x & 0xff)) {
		x >>= 8;
		r += 8;
	}
	if (!(x & 0xf)) {
		x >>= 4;
		r += 4;
	}
	if (!(x & 3)) {
		x >>= 2;
		r += 2;
	}
	if (!(x & 1)) {
		x >>= 1;
		r += 1;
	}
	return r;
}

/**
 * fls - find last (most-significant) bit set
 * @x: the word to search
 *
 * This is defined the same way as ffs.
 * Note fls(0) = 0, fls(1) = 1, fls(0x80000000) = 32.
 */
static inline int generic_fls(int x)
{
	int r = 32;

	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}


/* ---- ilog2 and fls ---- */

static inline int __ilog2_u32(uint32_t n)
{
    return generic_fls(n) - 1;
}

#define ilog2(n) \
    (__builtin_constant_p(n) ? \
        ((n) < 1 ? ____ilog2_NaN() : (63 - __builtin_clzll(n))) \
     : __ilog2_u32(n))

static inline int ____ilog2_NaN(void)
{
    panic("ilog2 invalid number");
}

/* ---- bit test ---- */
static inline int test_bit(long nr, const volatile unsigned long *addr)
{
    return ((*addr >> nr) & 1UL);
}

/* ---- atomic bit ops ---- */
static inline int test_and_set_bit(long nr, volatile unsigned long *addr)
{
    unsigned long mask = 1UL << nr;
    unsigned long old;
    __atomic_fetch_or(addr, mask, __ATOMIC_ACQ_REL);
    old = __atomic_load_n(addr, __ATOMIC_RELAXED);
    return !!(old & mask);
}

static inline int test_and_clear_bit(long nr, volatile unsigned long *addr)
{
    unsigned long mask = 1UL << nr;
    unsigned long old;
    __atomic_fetch_and(addr, ~mask, __ATOMIC_ACQ_REL);
    old = __atomic_load_n(addr, __ATOMIC_RELAXED);
    return !!(old & mask);
}

#endif
