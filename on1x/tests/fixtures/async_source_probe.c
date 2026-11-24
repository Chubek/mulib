/* Observe emitted-code execution without substituting for the scheduler,
 * allocations, or frame callbacks. */
#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mulib-fiber.h>
#include "on1x_async_frame.h"

static uint64_t now_ms;
static unsigned stacks, allocations, releases;

int __real_clock_gettime(clockid_t,struct timespec *);
int __wrap_clock_gettime(clockid_t id,struct timespec *out){
 if(id!=CLOCK_MONOTONIC)return __real_clock_gettime(id,out);
 out->tv_sec=(time_t)(now_ms/1000);
 out->tv_nsec=(long)((now_ms%1000)*1000000);
 return 0;
}
int __wrap_nanosleep(const struct timespec *delay,struct timespec *unused){
 (void)unused;
 now_ms+=(uint64_t)delay->tv_sec*1000+(uint64_t)delay->tv_nsec/1000000;
 return 0;
}
static void report(void){
 printf("probe: %" PRIu64 " %u %u %u\n",now_ms,stacks,allocations,releases);
}
int __real_mulib_fiber_spawn(mulib_fiber_scheduler_t *,mulib_fiber_fn,void *,size_t,mulib_fiber_t **);
int __wrap_mulib_fiber_spawn(mulib_fiber_scheduler_t *s,mulib_fiber_fn f,void *a,size_t size,mulib_fiber_t **out){
 if(++stacks==1&&atexit(report)!=0)abort();
 return __real_mulib_fiber_spawn(s,f,a,size,out);
}
void *__real_on1x_async_frame_alloc_v1(size_t,const char *,size_t,size_t);
void *__wrap_on1x_async_frame_alloc_v1(size_t size,const char *file,size_t line,size_t column){
 ++allocations;
 return __real_on1x_async_frame_alloc_v1(size,file,line,column);
}
void __real_on1x_async_frame_free_v1(void *);
void __wrap_on1x_async_frame_free_v1(void *frame){
 ++releases;
 __real_on1x_async_frame_free_v1(frame);
}
