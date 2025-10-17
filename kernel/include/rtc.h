#ifndef RTC_H
#define RTC_H
// System call for gettimeofday
#include "kernel/types.h"

struct timeval {
    uint64_t tv_sec;
    uint64_t tv_usec;
};

extern struct spinlock rtc_lock;
#endif