#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

static struct timespec fixture_time;
static int fixture_failure;
#ifdef CLOCK_MONOTONIC
static int fixture_clock(clockid_t id,struct timespec *out) {
  (void)id;*out=fixture_time;return fixture_failure?-1:0;
}
#define clock_gettime fixture_clock
#else
static int fixture_clock(struct timespec *out,int base) {
  (void)base;*out=fixture_time;return fixture_failure?0:TIME_UTC;
}
#define timespec_get fixture_clock
#endif
/* Observe the adapter's fatal path without producing a core dump. */
static _Noreturn void fixture_abort(void) { exit(73); }
#define abort fixture_abort
#include "../stdppx/runtime/on1x_ppx_timing.c"

int main(int argc,char **argv) {
  if(argc!=2)return 2;
  int mode=atoi(argv[1]);
  fixture_time.tv_sec=(time_t)(INT64_MAX/INT64_C(1000000000));
  fixture_time.tv_nsec=(long)(INT64_MAX%INT64_C(1000000000));
  switch(mode) {
    case 0: break;
    case 1: ++fixture_time.tv_nsec;break;
    case 2: ++fixture_time.tv_sec;fixture_time.tv_nsec=0;break;
    case 3: fixture_time.tv_sec=-1;break;
    case 4: fixture_time.tv_nsec=-1;break;
    case 5: fixture_time.tv_nsec=1000000000L;break;
    case 6: fixture_failure=1;break;
    case 7: fixture_time.tv_sec=0;fixture_time.tv_nsec=0;break;
    default: return 2;
  }
  int64_t value=on1x_ppx_clock_ns();
  assert(mode==0||mode==7);
  assert(value==(mode==0?INT64_MAX:0));
  return 0;
}
