#ifndef XV6_FIND_BITS_H_
#define XV6_FIND_BITS_H_

unsigned long find_next_zero_bit(const unsigned long *addr, unsigned long size, unsigned long offset);

#define BITS_PER_LONG 32
#define BITOP_WORD(nr) ((nr) / BITS_PER_LONG)

/**
 * __ffs - find first set bit in word (1-indexed)
 * @word: The word to search
 * Returns 0 if no bits set, otherwise position (1-32)
 */
static inline unsigned long __ffs(unsigned long word)
{
    if (word == 0)
        return 0;
    
    unsigned long ret = 1;
    while ((word & 1UL) == 0) {
        word >>= 1;
        ret++;
    }
    return ret;
}

/**
 * ffz - find first zero bit in word (1-indexed)
 * @word: The word to search
 * Returns 0 if all bits set, otherwise position (1-32)
 */
static inline unsigned long ffz(unsigned long word)
{
    if (word == ~0UL)
        return 0;
    return __ffs(~word);
}


#endif /* XV6_FIND_BITS_H_ */

