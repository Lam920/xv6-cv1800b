#include "include/find_bits.h"

/**
 * find_next_zero_bit - find next zero bit in bitmap
 * @addr: pointer to bitmap array
 * @size: size of bitmap in bits
 * @offset: starting bit offset
 * Returns position of next zero bit, or size if none found
 */



// portable count trailing zeros (ctz) for unsigned long
static inline unsigned long riscv_ctzl(unsigned long x)
{
    unsigned long r = 0;

    if (x == 0)
        return sizeof(unsigned long) * 8; // return max if no set bits

    while ((x & 1UL) == 0) {
        r++;
        x >>= 1;
    }
    return r;
}

// find first zero bit at or after 'offset' in bitmap 'addr'
unsigned long
find_next_zero_bit(const unsigned long *addr,
                   unsigned long size,
                   unsigned long offset)
{
    unsigned long idx, bitpos;
    unsigned long word;

    if (offset >= size)
        return size;

    idx = offset / BITS_PER_LONG;
    bitpos = offset % BITS_PER_LONG;

    // Invert bits so zeros become ones
    word = ~addr[idx];

    // Mask out bits before offset
    word &= (~0UL << bitpos);

    while (1) {
        if (word) {
            unsigned long bit = riscv_ctzl(word);
            unsigned long res = idx * BITS_PER_LONG + bit;
            if (res < size)
                return res;
            return size;
        }
        idx++;
        if (idx * BITS_PER_LONG >= size)
            break;
        word = ~addr[idx];
    }
    return size;
}