#ifndef XV6_FIND_BITS_H_
#define XV6_FIND_BITS_H_

unsigned long find_next_zero_bit(const unsigned long *addr, unsigned long size, unsigned long offset);

#define BITS_PER_LONG 32

#define BITOP_WORD(nr) ((nr) / BITS_PER_LONG)

/**
 * __ffs - find first set bit in word
 * @word: The word to search
 *
 * Undefined if @word is 0 — caller should check first.
 */
static inline unsigned long __ffs(unsigned long word)
{
    unsigned long ret = 0;

    // Loop until we find the first bit set
    while ((word & 1UL) == 0) {
        word >>= 1;
        ret++;
    }
    return ret;
}

/**
 * ffz - find first zero bit in word
 * @word: The word to search
 *
 * Undefined if no zero exists — caller should check against ~0UL first.
 */
static inline unsigned long ffz(unsigned long word)
{
    // Just invert and reuse __ffs
    return __ffs(~word);
}


#endif /* XV6_FIND_BITS_H_ */

