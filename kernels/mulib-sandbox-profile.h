/** @file mulib-sandbox-profile.h
 * Versioned Linux plugin limits and enforcement requirements. C11 single-header.
 * Define MULIB_IMPL_SANDBOX_PROFILE once. Limits are explicit; zero is not unlimited.
 */
#ifndef MULIB_SANDBOX_PROFILE_H
#define MULIB_SANDBOX_PROFILE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_SANDBOX_PROFILE_VERSION 1u
#define MULIB_SANDBOX_IPC_FD 3
#define MULIB_SANDBOX_SETUP_FD 4
#define MULIB_SANDBOX_MIB UINT64_C(1048576)
enum { MULIB_SB_OK=0, MULIB_SB_AGAIN=1, MULIB_SB_UNCERTAIN=2,
       MULIB_SB_INVALID=-1, MULIB_SB_SYSTEM=-2, MULIB_SB_UNSUPPORTED=-3,
       MULIB_SB_DENIED=-4, MULIB_SB_LIMIT=-5, MULIB_SB_TIMEOUT=-6,
       MULIB_SB_PROTOCOL=-7, MULIB_SB_NOMEM=-8, MULIB_SB_STATE=-9,
       MULIB_SB_EOF=-10 };
enum { MULIB_SB_ENFORCE_SECCOMP=1u, MULIB_SB_ENFORCE_RLIMIT=2u,
       MULIB_SB_ENFORCE_CREDENTIALS=4u, MULIB_SB_ENFORCE_LANDLOCK=8u,
       MULIB_SB_ENFORCE_NAMESPACE=16u, MULIB_SB_ENFORCE_CGROUP=32u };
enum { MULIB_SB_NS_USER=1u, MULIB_SB_NS_MOUNT=2u, MULIB_SB_NS_NETWORK=4u,
       MULIB_SB_NS_IPC=8u, MULIB_SB_NS_UTS=16u, MULIB_SB_NS_PID=32u };
#define MULIB_SB_NS_ALL 63u
#define MULIB_SB_ENFORCE_ALL 63u
#define MULIB_SB_BASELINE 7u
/** Version 1 strict worker: one broker fd, no file/network/process creation.
 * memory_bytes is RLIMIT_AS; cgroup_memory_bytes is a separate hard cap.
 * CPU seconds is cumulative per process; cpu_quota_us/period are cgroup rate.
 */
typedef struct mulib_sandbox_profile {
    uint32_t version, required, namespace_flags, landlock_min_abi;
    uint64_t memory_bytes, stack_bytes, cpu_seconds, wall_ms, startup_ms;
    uint64_t cgroup_memory_bytes, cpu_quota_us, cpu_period_us;
    uint32_t max_processes, max_open_files, max_frame_bytes, max_broker_requests;
    uint64_t max_broker_bytes;
} mulib_sandbox_profile_t;
mulib_sandbox_profile_t mulib_sandbox_profile_default(void);
int mulib_sandbox_profile_validate(const mulib_sandbox_profile_t *p);
const char *mulib_sandbox_strerror(int code);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SANDBOX_PROFILE) && !defined(MULIB_SANDBOX_PROFILE_IMPLEMENTED)
#define MULIB_SANDBOX_PROFILE_IMPLEMENTED
mulib_sandbox_profile_t mulib_sandbox_profile_default(void) {
    mulib_sandbox_profile_t p = {0};
    p.version=1; p.required=MULIB_SB_BASELINE; p.memory_bytes=256*MULIB_SANDBOX_MIB;
    p.stack_bytes=8*MULIB_SANDBOX_MIB; p.cpu_seconds=30; p.wall_ms=60000; p.startup_ms=3000;
    p.cgroup_memory_bytes=256*MULIB_SANDBOX_MIB; p.cpu_quota_us=100000; p.cpu_period_us=100000;
    p.max_processes=1; p.max_open_files=4; p.max_frame_bytes=2*1024*1024;
    p.max_broker_requests=256; p.max_broker_bytes=16*MULIB_SANDBOX_MIB;
    p.landlock_min_abi=3; return p;
}
int mulib_sandbox_profile_validate(const mulib_sandbox_profile_t *p) {
    if (!p || p->version!=1 || (p->required & ~MULIB_SB_ENFORCE_ALL) ||
        (p->required & MULIB_SB_BASELINE)!=MULIB_SB_BASELINE ||
        (p->namespace_flags & ~MULIB_SB_NS_ALL) ||
        (!!p->namespace_flags != !!(p->required&MULIB_SB_ENFORCE_NAMESPACE)) ||
        p->landlock_min_abi<1 || p->landlock_min_abi>UINT32_C(65535) ||
        p->memory_bytes<16*MULIB_SANDBOX_MIB || p->stack_bytes<65536 || p->stack_bytes>p->memory_bytes ||
        !p->cpu_seconds || p->cpu_seconds>86400 || !p->wall_ms || p->wall_ms>86400000 ||
        !p->startup_ms || p->startup_ms>600000 || p->max_processes!=1 || p->max_open_files!=4 ||
        p->max_frame_bytes<8 || p->max_frame_bytes>64*1024*1024 || !p->max_broker_requests ||
        !p->max_broker_bytes || p->max_broker_bytes<p->max_frame_bytes ||
        ((p->required&MULIB_SB_ENFORCE_CGROUP) && (!p->cgroup_memory_bytes ||
           p->cpu_period_us<1000 || p->cpu_period_us>1000000 || p->cpu_quota_us<1000 ||
           p->cpu_quota_us>UINT64_C(1000000000)))) return MULIB_SB_INVALID;
    return 0;
}
const char *mulib_sandbox_strerror(int c) {
    switch(c){case 0:return "ok";case 1:return "again";case 2:return "commit uncertain";
    case -1:return "invalid argument";case -2:return "system error";case -3:return "unsupported";
    case -4:return "denied";case -5:return "limit exceeded";case -6:return "deadline exceeded";
    case -7:return "protocol error";case -8:return "out of memory";case -9:return "invalid state";
    case -10:return "end of stream";default:return "unknown status";}
}
#endif
