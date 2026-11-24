/** @file mulib-landlock.h
 * Linux Landlock filesystem allowlist. Caller owns O_PATH rule descriptors.
 * Define MULIB_IMPL_LANDLOCK and MULIB_IMPL_SANDBOX_PROFILE once.
 */
#ifndef MULIB_LANDLOCK_H
#define MULIB_LANDLOCK_H
#include "mulib-sandbox-profile.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Stable Linux UAPI bit assignments, not host-header-dependent. */
#define MULIB_LANDLOCK_EXECUTE (UINT64_C(1)<<0)
#define MULIB_LANDLOCK_WRITE_FILE (UINT64_C(1)<<1)
#define MULIB_LANDLOCK_READ_FILE (UINT64_C(1)<<2)
#define MULIB_LANDLOCK_READ_DIR (UINT64_C(1)<<3)
#define MULIB_LANDLOCK_REMOVE_DIR (UINT64_C(1)<<4)
#define MULIB_LANDLOCK_REMOVE_FILE (UINT64_C(1)<<5)
#define MULIB_LANDLOCK_MAKE_CHAR (UINT64_C(1)<<6)
#define MULIB_LANDLOCK_MAKE_DIR (UINT64_C(1)<<7)
#define MULIB_LANDLOCK_MAKE_REG (UINT64_C(1)<<8)
#define MULIB_LANDLOCK_MAKE_SOCK (UINT64_C(1)<<9)
#define MULIB_LANDLOCK_MAKE_FIFO (UINT64_C(1)<<10)
#define MULIB_LANDLOCK_MAKE_BLOCK (UINT64_C(1)<<11)
#define MULIB_LANDLOCK_MAKE_SYM (UINT64_C(1)<<12)
#define MULIB_LANDLOCK_REFER (UINT64_C(1)<<13)
#define MULIB_LANDLOCK_TRUNCATE (UINT64_C(1)<<14)
#define MULIB_LANDLOCK_IOCTL_DEV (UINT64_C(1)<<15)
typedef struct mulib_landlock_rule {int fd;uint64_t access;} mulib_landlock_rule_t;
/** Returns positive ABI or a negative status. */
int mulib_landlock_abi(void);
uint64_t mulib_landlock_supported_access(unsigned abi);
/** Handles every supported filesystem right known through ABI 5. No rules
 * means deny all handled access. No automatic fallback. Irreversible.
 * Does not revoke existing fds or mediate all metadata/path operations. */
int mulib_landlock_restrict(const mulib_landlock_rule_t *rules,size_t count,unsigned min_abi);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LANDLOCK) && !defined(MULIB_LANDLOCK_IMPLEMENTED)
#define MULIB_LANDLOCK_IMPLEMENTED
uint64_t mulib_landlock_supported_access(unsigned a){if(!a)return 0;uint64_t m=(UINT64_C(1)<<13)-1;if(a>=2)m|=MULIB_LANDLOCK_REFER;if(a>=3)m|=MULIB_LANDLOCK_TRUNCATE;if(a>=5)m|=MULIB_LANDLOCK_IOCTL_DEV;return m;}
#if defined(__linux__)
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif
#if defined(__linux__) && defined(SYS_landlock_create_ruleset) && defined(SYS_landlock_add_rule) && defined(SYS_landlock_restrict_self)
int mulib_landlock_abi(void){long a=syscall(SYS_landlock_create_ruleset,NULL,0,1u);return a>0?(int)a:(errno==ENOSYS||errno==EOPNOTSUPP?MULIB_SB_UNSUPPORTED:MULIB_SB_SYSTEM);}
int mulib_landlock_restrict(const mulib_landlock_rule_t*r,size_t n,unsigned min){if((!r&&n)||n>4096||!min)return MULIB_SB_INVALID;int a=mulib_landlock_abi();if(a<0)return a;if((unsigned)a<min)return MULIB_SB_UNSUPPORTED;uint64_t handled=mulib_landlock_supported_access((unsigned)a);
    for(size_t i=0;i<n;i++){struct stat st;int flags;if(r[i].fd<0||!r[i].access||(r[i].access&~handled)||(flags=fcntl(r[i].fd,F_GETFL))<0||!(flags&O_PATH)||fstat(r[i].fd,&st))return MULIB_SB_INVALID;if(!S_ISDIR(st.st_mode)&&(r[i].access&~(MULIB_LANDLOCK_EXECUTE|MULIB_LANDLOCK_WRITE_FILE|MULIB_LANDLOCK_READ_FILE|MULIB_LANDLOCK_TRUNCATE|MULIB_LANDLOCK_IOCTL_DEV)))return MULIB_SB_INVALID;}
    struct {uint64_t handled_access_fs;} attr={handled};long fd=syscall(SYS_landlock_create_ruleset,&attr,sizeof attr,0u);if(fd<0)return MULIB_SB_SYSTEM;
    int rc=0;for(size_t i=0;i<n;i++){/* UAPI packed path-beneath struct is 12 bytes. */
        struct __attribute__((packed)) {uint64_t access;int32_t parent_fd;} rule={r[i].access,r[i].fd};
        if(syscall(SYS_landlock_add_rule,(int)fd,1,&rule,0u)){rc=MULIB_SB_SYSTEM;break;}}
    if(!rc&&(prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0)||syscall(SYS_landlock_restrict_self,(int)fd,0u)))rc=MULIB_SB_SYSTEM;
    (void)close((int)fd);return rc;}
#else
int mulib_landlock_abi(void){return MULIB_SB_UNSUPPORTED;}
int mulib_landlock_restrict(const mulib_landlock_rule_t*r,size_t n,unsigned m){(void)r;(void)n;(void)m;return MULIB_SB_UNSUPPORTED;}
#endif
#endif
