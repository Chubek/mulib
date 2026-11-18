#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

/* Only the adapter's clock changes. Ordinary time advances when the real MuLIB
 * scheduler reaches the supervisor's idle path; busy mode instead ticks once
 * per observation to force expiry without a real-time scheduler race. */
static uint64_t ms;
int __real_clock_gettime(clockid_t,struct timespec *);
int __wrap_clock_gettime(clockid_t clock,struct timespec *now){
 const char *mode=getenv("ON1X_TEST_WITHIN_CLOCK");
 if(clock!=CLOCK_MONOTONIC)return __real_clock_gettime(clock,now);
 if(mode&&*mode=='f'){errno=EIO;return -1;}
 if(mode&&*mode=='o')ms=UINT64_MAX-1000;
 if(mode&&*mode=='b')++ms;
 now->tv_sec=(time_t)(ms/1000);
 now->tv_nsec=(long)((ms%1000)*1000000);
 return 0;
}
int __wrap_nanosleep(const struct timespec *delay,struct timespec *remaining){
 (void)remaining;
 ms+=(uint64_t)delay->tv_sec*1000+(uint64_t)delay->tv_nsec/1000000;
 return 0;
}
