#ifndef INC_TYPES_H
#define INC_TYPES_H

#include "errno.h"

typedef unsigned int    uint;
typedef unsigned short  ushort;
typedef unsigned char   uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef unsigned long uint64;

typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;
typedef unsigned long   uint64_t;
typedef unsigned long   uintptr_t;
typedef unsigned long   size_t;

typedef short       int16_t;
typedef int         int32_t;
typedef long        int64_t;
typedef long        ssize_t;
typedef long		time_t;

typedef uint64_t    pde_t;
typedef int         boot;

// typedef uint64_t    dma_addr_t;

typedef uint32_t    __be32;

typedef int         bool;
#define true        1
#define false       0

#define NULL        ((void *)0)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define uswap_16(x) \
	((((x) & 0xff00) >> 8) | \
	 (((x) & 0x00ff) << 8))
#define uswap_32(x) \
	((((x) & 0xff000000) >> 24) | \
	 (((x) & 0x00ff0000) >>  8) | \
	 (((x) & 0x0000ff00) <<  8) | \
	 (((x) & 0x000000ff) << 24))
#define _uswap_64(x, sfx) \
	((((x) & 0xff00000000000000##sfx) >> 56) | \
	 (((x) & 0x00ff000000000000##sfx) >> 40) | \
	 (((x) & 0x0000ff0000000000##sfx) >> 24) | \
	 (((x) & 0x000000ff00000000##sfx) >>  8) | \
	 (((x) & 0x00000000ff000000##sfx) <<  8) | \
	 (((x) & 0x0000000000ff0000##sfx) << 24) | \
	 (((x) & 0x000000000000ff00##sfx) << 40) | \
	 (((x) & 0x00000000000000ff##sfx) << 56))
#if defined(__GNUC__)
# define uswap_64(x) _uswap_64(x, ull)
#else
# define uswap_64(x) _uswap_64(x, )
#endif

# define cpu_to_le16(x)		(x)
# define cpu_to_le32(x)		(x)
# define cpu_to_le64(x)		(x)
# define le16_to_cpu(x)		(x)
# define le32_to_cpu(x)		(x)
# define le64_to_cpu(x)		(x)
# define cpu_to_be16(x)		uswap_16(x)
# define cpu_to_be32(x)		uswap_32(x)
# define cpu_to_be64(x)		uswap_64(x)
# define be16_to_cpu(x)		uswap_16(x)
# define be32_to_cpu(x)		uswap_32(x)
# define be64_to_cpu(x)		uswap_64(x)


#ifndef CONFIG_DM_ETH
#define CONFIG_DM_ETH   1
#endif

typedef signed char s8;
typedef unsigned char u8;

typedef signed short s16;
typedef unsigned short u16;

typedef signed int s32;
typedef unsigned int u32;

typedef signed long long s64;
typedef unsigned long long u64;

#define BITS_PER_LONG 32

/* Dma addresses are 32-bits wide.  */

typedef unsigned long phys_addr_t;
typedef unsigned long phys_size_t;

typedef unsigned long ulong;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#ifndef EOK
#define EOK             0      /* no error */
#endif //EOK

#ifndef EPERM
#define EPERM           1      /* Operation not permitted */
#endif //EPERM

#ifndef ENOENT
#define ENOENT          2      /* No such file or directory */
#endif //ENOENT

#ifndef EIO
#define EIO             5      /* I/O error */
#endif //EIO

#ifndef ENOEXEC
#define ENOEXEC         8      /* Exec format error */
#endif //ENOEXEC

#ifndef EAGAIN
#define EAGAIN          11      /* Try again */
#endif //EAGAIN

#ifndef ENOMEM
#define ENOMEM          12      /* Out of memory */
#endif //ENOMEM

#ifndef EFAULT
#define EFAULT          14      /* Bad address */
#endif //EFAULT

#ifndef EBUSY
#define EBUSY           16      /* Device or resource busy */
#endif //EBUSY

#ifndef EINVAL
#define EINVAL          22      /* Invalid argument */
#endif //EINVAL

#ifndef ENOSPC
#define ENOSPC          28      /* No space left on device */
#endif //ENOSPC

#ifndef EBADSLT
#define EBADSLT         57      /* Invalid slot */
#endif //EBADSLT

#ifndef EPROTO
#define EPROTO          71      /* Protocol error */
#endif //EPROTO

#ifndef EOVERFLOW
#define EOVERFLOW       75      /* Value too large for defined data type */
#endif //EOVERFLOW

#ifndef EOPNOTSUPP
#define EOPNOTSUPP      95      /* Operation not supported */
#endif //EOPNOTSUPP

#ifndef ESHUTDOWN
#define ESHUTDOWN       108     /* Cannot send after transport endpoint shutdown */
#endif

#ifndef ETIMEDOUT
#define ETIMEDOUT       110     /* Connection timed out */
#endif //ETIMEDOUT

#ifndef EINPROGRESS
#define EINPROGRESS     115     /* Operation now in progress */
#endif //EINPROGRESS

#ifndef EDQUOT
#define EDQUOT          122     /* Quota exceeded */
#endif //EDQUOT

#ifndef ENOTSUP
#define ENOTSUP         EOPNOTSUPP
#endif //ENOTSUP

#ifndef ECANCELED
#define ECANCELED       126      /* Cancelled */
#endif //ECANCELED


#endif
