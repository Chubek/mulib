/** @file mulib-namespace.h Linux namespace setup; irreversible child-only API.
 * Define MULIB_IMPL_NAMESPACE and MULIB_IMPL_SANDBOX_PROFILE once.
 */
#ifndef MULIB_NAMESPACE_H
#define MULIB_NAMESPACE_H
#include "mulib-sandbox-profile.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_namespace_spec {uint32_t flags;uint32_t inside_uid,inside_gid;} mulib_namespace_spec_t;
/** Success reports completed steps. PID applies to FUTURE children only.
 * On failure the process may already be changed; exit, never continue. No
 * rootfs pivot, mount construction or supplementary group guarantee implied. */
int mulib_namespace_enter(const mulib_namespace_spec_t *spec,uint32_t *applied);
/** Bind-mount propagation isolated first by enter(MOUNT); make root tree private. */
int mulib_namespace_mount_private(void);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NAMESPACE) && !defined(MULIB_NAMESPACE_IMPLEMENTED)
#define MULIB_NAMESPACE_IMPLEMENTED
#if defined(__linux__)
#include <sched.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
static int mulib_namespace_write_(const char*path,const char*p,size_t n){int fd=open(path,O_WRONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return MULIB_SB_SYSTEM;ssize_t w;do{w=write(fd,p,n);}while(w<0&&errno==EINTR);int rc=w==(ssize_t)n?0:MULIB_SB_SYSTEM;if(close(fd)&&!rc)rc=MULIB_SB_SYSTEM;return rc;}
int mulib_namespace_mount_private(void){return mount(NULL,"/",NULL,MS_REC|MS_PRIVATE,NULL)?MULIB_SB_SYSTEM:0;}
int mulib_namespace_enter(const mulib_namespace_spec_t*s,uint32_t*out){if(!s||!out||!s->flags||(s->flags&~MULIB_SB_NS_ALL)||s->inside_uid==UINT32_MAX||s->inside_gid==UINT32_MAX)return MULIB_SB_INVALID;uint32_t done=0;*out=0;
    if(s->flags&MULIB_SB_NS_USER){uid_t uid=geteuid();gid_t gid=getegid();char b[96];int n;if(unshare(CLONE_NEWUSER))return MULIB_SB_SYSTEM;
        n=snprintf(b,sizeof b,"%u %lu 1\n",s->inside_uid,(unsigned long)uid);if(n<0||(size_t)n>=sizeof b||mulib_namespace_write_("/proc/self/uid_map",b,(size_t)n))return MULIB_SB_SYSTEM;
        if(mulib_namespace_write_("/proc/self/setgroups","deny",4))return MULIB_SB_SYSTEM;
        n=snprintf(b,sizeof b,"%u %lu 1\n",s->inside_gid,(unsigned long)gid);if(n<0||(size_t)n>=sizeof b||mulib_namespace_write_("/proc/self/gid_map",b,(size_t)n))return MULIB_SB_SYSTEM;done|=MULIB_SB_NS_USER;*out=done;}
    int flags=0;if(s->flags&MULIB_SB_NS_MOUNT)flags|=CLONE_NEWNS;if(s->flags&MULIB_SB_NS_NETWORK)flags|=CLONE_NEWNET;if(s->flags&MULIB_SB_NS_IPC)flags|=CLONE_NEWIPC;if(s->flags&MULIB_SB_NS_UTS)flags|=CLONE_NEWUTS;if(s->flags&MULIB_SB_NS_PID)flags|=CLONE_NEWPID;
    if(flags&&unshare(flags))return MULIB_SB_SYSTEM;
    done|=s->flags&~MULIB_SB_NS_MOUNT;*out=done;
    if(s->flags&MULIB_SB_NS_MOUNT){if(mulib_namespace_mount_private())return MULIB_SB_SYSTEM;*out|=MULIB_SB_NS_MOUNT;}return 0;}
#else
int mulib_namespace_enter(const mulib_namespace_spec_t*s,uint32_t*out){(void)s;(void)out;return MULIB_SB_UNSUPPORTED;}int mulib_namespace_mount_private(void){return MULIB_SB_UNSUPPORTED;}
#endif
#endif
