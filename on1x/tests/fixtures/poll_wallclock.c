#include <time.h>
/* An independent link-time UTC clock perturbation. CLOCK_MONOTONIC is real. */
int __wrap_timespec_get(struct timespec *value,int base){
 static unsigned calls;
 if(base!=TIME_UTC)return 0;
 value->tv_sec=(calls++&1u)?4000000000L:0;
 value->tv_nsec=0;
 return base;
}
