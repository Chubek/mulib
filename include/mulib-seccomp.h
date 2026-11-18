/** @file mulib-seccomp.h
 * Linux seccomp classic-BPF builder. Default deny, architecture checked.
 * Strict worker profile for trusted single-thread bootstrap. Define
 * MULIB_IMPL_SECCOMP and MULIB_IMPL_SANDBOX_PROFILE once.
 */
#ifndef MULIB_SECCOMP_H
#define MULIB_SECCOMP_H
#include "mulib-sandbox-profile.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_SECCOMP_MAX_RULES 128u
#define MULIB_SECCOMP_MAX_CHECKS 4u
typedef struct mulib_seccomp_check {uint32_t argument;uint64_t mask,value;} mulib_seccomp_check_t;
typedef struct mulib_seccomp_rule {int32_t syscall_nr;uint32_t count;mulib_seccomp_check_t checks[MULIB_SECCOMP_MAX_CHECKS];} mulib_seccomp_rule_t;
typedef struct mulib_seccomp_policy {uint32_t count;int deny_errno;mulib_seccomp_rule_t rules[MULIB_SECCOMP_MAX_RULES];} mulib_seccomp_policy_t;
/** deny_errno=0 kills process; 1..4095 returns errno on unmatched syscall. */
int mulib_seccomp_init(mulib_seccomp_policy_t *p,int deny_errno);
int mulib_seccomp_allow(mulib_seccomp_policy_t *p,int32_t nr,const mulib_seccomp_check_t *checks,size_t count);
int mulib_seccomp_validate(const mulib_seccomp_policy_t *p);
/** Construct strict no-file/no-network/no-exec policy for a connected stream. */
int mulib_seccomp_worker(mulib_seccomp_policy_t *p,int fd,int deny_errno);
int mulib_seccomp_supported(void);
/** Irreversible; sets no_new_privs, uses TSYNC. Require trusted single-thread caller. */
int mulib_seccomp_install(const mulib_seccomp_policy_t *p);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECCOMP) && !defined(MULIB_SECCOMP_IMPLEMENTED)
#define MULIB_SECCOMP_IMPLEMENTED
#include <string.h>
int mulib_seccomp_init(mulib_seccomp_policy_t*p,int e){if(!p||e<0||e>4095)return MULIB_SB_INVALID;memset(p,0,sizeof *p);p->deny_errno=e;return 0;}
static int mulib_seccomp_rule_valid_(const mulib_seccomp_rule_t*r){if(r->syscall_nr<0||r->syscall_nr>=0x40000000||r->count>MULIB_SECCOMP_MAX_CHECKS)return 0;for(uint32_t i=0;i<r->count;i++)if(r->checks[i].argument>=6||!r->checks[i].mask||(r->checks[i].value&~r->checks[i].mask))return 0;return 1;}
int mulib_seccomp_validate(const mulib_seccomp_policy_t*p){if(!p||p->count>MULIB_SECCOMP_MAX_RULES||p->deny_errno<0||p->deny_errno>4095)return MULIB_SB_INVALID;for(uint32_t i=0;i<p->count;i++)if(!mulib_seccomp_rule_valid_(&p->rules[i]))return MULIB_SB_INVALID;return 0;}
int mulib_seccomp_allow(mulib_seccomp_policy_t*p,int32_t nr,const mulib_seccomp_check_t*c,size_t n){if(mulib_seccomp_validate(p)||(!c&&n)||n>MULIB_SECCOMP_MAX_CHECKS)return MULIB_SB_INVALID;if(p->count==MULIB_SECCOMP_MAX_RULES)return MULIB_SB_LIMIT;mulib_seccomp_rule_t r={0};r.syscall_nr=nr;r.count=(uint32_t)n;if(n)memcpy(r.checks,c,n*sizeof *c);if(!mulib_seccomp_rule_valid_(&r))return MULIB_SB_INVALID;p->rules[p->count++]=r;return 0;}
#if defined(__linux__) && defined(__LP64__) && (defined(__x86_64__) || defined(__aarch64__)) && defined(__BYTE_ORDER__) && __BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
int mulib_seccomp_supported(void){uint32_t action=SECCOMP_RET_KILL_PROCESS;return syscall(SYS_seccomp,SECCOMP_GET_ACTION_AVAIL,0,&action)==0;}
int mulib_seccomp_worker(mulib_seccomp_policy_t*p,int fd,int e){if(fd<3||mulib_seccomp_init(p,e))return MULIB_SB_INVALID;
    mulib_seccomp_check_t f={0,UINT64_MAX,(uint64_t)fd};
#define MULIB_SC_FD(n) do{if(mulib_seccomp_allow(p,SYS_##n,&f,1))return MULIB_SB_LIMIT;}while(0)
#define MULIB_SC_ALLOW(n) do{if(mulib_seccomp_allow(p,SYS_##n,NULL,0))return MULIB_SB_LIMIT;}while(0)
    MULIB_SC_FD(read);MULIB_SC_FD(write);
    /* Restrict both socket calls to connected-stream I/O, with no address
       arguments and only the flags required by the framed transport. */
    mulib_seccomp_check_t io[4]={f,{3,UINT64_C(0xffffffff),MSG_DONTWAIT|MSG_NOSIGNAL},
                               {4,UINT64_MAX,0},{5,UINT64_C(0xffffffff),0}};
    if(mulib_seccomp_allow(p,SYS_sendto,io,4))return MULIB_SB_LIMIT;
    io[1].value=MSG_DONTWAIT;io[3].mask=UINT64_MAX;
    if(mulib_seccomp_allow(p,SYS_recvfrom,io,4))return MULIB_SB_LIMIT;
    /* Admit only private anonymous mappings without other mapping modifiers. */
    mulib_seccomp_check_t mp={2,UINT64_C(0xffffffff)&~(uint64_t)(PROT_READ|PROT_WRITE),0};
    mulib_seccomp_check_t mm[2]={mp,{3,UINT64_C(0xffffffff),MAP_ANONYMOUS|MAP_PRIVATE}};
    if(mulib_seccomp_allow(p,SYS_mmap,mm,2)||mulib_seccomp_allow(p,SYS_mprotect,&mp,1))return MULIB_SB_LIMIT;
    MULIB_SC_ALLOW(brk);MULIB_SC_ALLOW(munmap);MULIB_SC_ALLOW(madvise);MULIB_SC_ALLOW(futex);
    MULIB_SC_ALLOW(clock_gettime);MULIB_SC_ALLOW(gettimeofday);MULIB_SC_ALLOW(getrandom);
    MULIB_SC_ALLOW(getpid);MULIB_SC_ALLOW(gettid);MULIB_SC_ALLOW(close);
    MULIB_SC_ALLOW(rt_sigaction);MULIB_SC_ALLOW(rt_sigprocmask);MULIB_SC_ALLOW(rt_sigreturn);MULIB_SC_ALLOW(sigaltstack);
    MULIB_SC_ALLOW(ppoll);
#ifdef SYS_poll
    MULIB_SC_ALLOW(poll);
#endif
    MULIB_SC_ALLOW(exit);MULIB_SC_ALLOW(exit_group);
#undef MULIB_SC_FD
#undef MULIB_SC_ALLOW
    return 0;
}
int mulib_seccomp_install(const mulib_seccomp_policy_t*p){if(mulib_seccomp_validate(p))return MULIB_SB_INVALID;if(!mulib_seccomp_supported())return MULIB_SB_UNSUPPORTED;
    struct sock_filter ins[4096];size_t n=0;
#define MULIB_SC_STMT(c,k) do{if(n==4096)return MULIB_SB_LIMIT;ins[n++]=(struct sock_filter)BPF_STMT((c),(k));}while(0)
#define MULIB_SC_JUMP(c,k,t,f) do{if(n==4096)return MULIB_SB_LIMIT;ins[n++]=(struct sock_filter)BPF_JUMP((c),(k),(t),(f));}while(0)
#ifdef __x86_64__
    uint32_t arch=AUDIT_ARCH_X86_64;
#else
    uint32_t arch=AUDIT_ARCH_AARCH64;
#endif
    MULIB_SC_STMT(BPF_LD|BPF_W|BPF_ABS,offsetof(struct seccomp_data,arch));
    MULIB_SC_JUMP(BPF_JMP|BPF_JEQ|BPF_K,arch,1,0);MULIB_SC_STMT(BPF_RET|BPF_K,SECCOMP_RET_KILL_PROCESS);
    for(uint32_t i=0;i<p->count;i++){
        const mulib_seccomp_rule_t*r=&p->rules[i];size_t jumps[9],nj=0;
        MULIB_SC_STMT(BPF_LD|BPF_W|BPF_ABS,offsetof(struct seccomp_data,nr));
        jumps[nj++]=n;MULIB_SC_JUMP(BPF_JMP|BPF_JEQ|BPF_K,(uint32_t)r->syscall_nr,0,0);
        for(uint32_t j=0;j<r->count;j++)for(unsigned half=0;half<2;half++){
            uint32_t mask=(uint32_t)(r->checks[j].mask>>(32*half));if(!mask)continue;
            uint32_t v=(uint32_t)(r->checks[j].value>>(32*half));
            MULIB_SC_STMT(BPF_LD|BPF_W|BPF_ABS,offsetof(struct seccomp_data,args)+(size_t)r->checks[j].argument*8+half*4);
            if(mask!=UINT32_MAX)MULIB_SC_STMT(BPF_ALU|BPF_AND|BPF_K,mask);
            jumps[nj++]=n;MULIB_SC_JUMP(BPF_JMP|BPF_JEQ|BPF_K,v,0,0);
        }
        MULIB_SC_STMT(BPF_RET|BPF_K,SECCOMP_RET_ALLOW);
        for(size_t j=0;j<nj;j++){size_t skip=n-jumps[j]-1;if(skip>255)return MULIB_SB_LIMIT;ins[jumps[j]].jf=(uint8_t)skip;}
    }
    MULIB_SC_STMT(BPF_RET|BPF_K,p->deny_errno?(SECCOMP_RET_ERRNO|(uint32_t)p->deny_errno):SECCOMP_RET_KILL_PROCESS);
    struct sock_fprog prog={(unsigned short)n,ins};
    if(prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0))return MULIB_SB_SYSTEM;
    long rc=syscall(SYS_seccomp,SECCOMP_SET_MODE_FILTER,SECCOMP_FILTER_FLAG_TSYNC,&prog);
    return rc==0?0:MULIB_SB_SYSTEM;
#undef MULIB_SC_STMT
#undef MULIB_SC_JUMP
}
#else
int mulib_seccomp_supported(void){return 0;}int mulib_seccomp_worker(mulib_seccomp_policy_t*p,int f,int e){(void)p;(void)f;(void)e;return MULIB_SB_UNSUPPORTED;}int mulib_seccomp_install(const mulib_seccomp_policy_t*p){(void)p;return MULIB_SB_UNSUPPORTED;}
#endif
#endif
