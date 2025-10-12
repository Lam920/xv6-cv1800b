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
static inline int test_bit(uint32_t nr, const void *addr) {
    const uint8_t *p = (const uint8_t *)addr;
    return (p[nr / 8] >> (nr % 8)) & 1;
}

#ifndef BITS_PER_LONG
#define BITS_PER_LONG (sizeof(unsigned long) * 8)
#endif

static inline int test_and_set_bit(int nr, volatile unsigned long *addr)
{
    uint32_t mask = 1U << (nr % 32);
    volatile uint32_t *p = ((volatile uint32_t *)addr) + (nr / 32);
    uint32_t old;

    __asm__ __volatile__ (
        "amoor.w %0, %2, (%1)"
        : "=r"(old)
        : "r"(p), "r"(mask)
        : "memory");

    return (old & mask) != 0;
}

static inline int test_and_clear_bit(int nr, volatile unsigned long *addr)
{
    uint32_t mask = 1U << (nr % 32);
    volatile uint32_t *p = ((volatile uint32_t *)addr) + (nr / 32);
    uint32_t old;

    __asm__ __volatile__ (
        "amoand.w %0, %2, (%1)"
        : "=r"(old)
        : "r"(p), "r"(~mask)
        : "memory");

    return (old & mask) != 0;
}


/* The `const' in roundup() prevents gcc-3.3 from calling __divdi3 */
#define roundup(x, y) (					\
{							\
	const typeof(y) __y = y;			\
	(((x) + (__y - 1)) / __y) * __y;		\
}							\
)
#define rounddown(x, y) (				\
{							\
	typeof(x) __x = (x);				\
	__x - (__x % (y));				\
}							\
)
#endif
