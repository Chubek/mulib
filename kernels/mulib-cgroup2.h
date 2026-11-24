/** @file mulib-cgroup2.h
 * Dedicated cgroup v2 child beneath an administrator-delegated directory.
 * No implicit delegation or controller changes. Define MULIB_IMPL_CGROUP2 once.
 */
#ifndef MULIB_CGROUP2_H
#define MULIB_CGROUP2_H
#include "mulib-sandbox-profile.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_cgroup2 {int parent_fd,fd;char name[128];} mulib_cgroup2_t;
#define MULIB_CGROUP2_INIT {-1,-1,{0}}
typedef struct mulib_cgroup2_limits {uint64_t memory_bytes,cpu_quota_us,cpu_period_us;uint32_t pids;int disable_swap;} mulib_cgroup2_limits_t;
/** Create exclusively; write and verify limits. Parent must be a cgroup2 fd. */
int mulib_cgroup2_create(mulib_cgroup2_t *g,int parent_fd,const char *name,const mulib_cgroup2_limits_t *limits);
int mulib_cgroup2_attach(const mulib_cgroup2_t *g,long pid);
/** Read back exact requested limits (memory rounded down to page size). */
int mulib_cgroup2_verify(const mulib_cgroup2_t *g,const mulib_cgroup2_limits_t *limits);
int mulib_cgroup2_populated(const mulib_cgroup2_t *g,int *populated);
int mulib_cgroup2_kill(const mulib_cgroup2_t *g);
/** Remove only empty leaf; retains handle on failure. */
int mulib_cgroup2_destroy(mulib_cgroup2_t *g);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CGROUP2) && !defined(MULIB_CGROUP2_IMPLEMENTED)
#define MULIB_CGROUP2_IMPLEMENTED
#if defined(__linux__)
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <linux/magic.h>
static int mulib_cgroup2_name_(const char*s){if(!s||!*s||strlen(s)>=128||!strcmp(s,".")||!strcmp(s,".."))return 0;for(;*s;s++)if(!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||*s=='-'||*s=='_'||*s=='.'))return 0;return 1;}
static int mulib_cgroup2_fs_(int fd){struct statfs fs;struct stat st;return fd>=0&&!fstatfs(fd,&fs)&&fs.f_type==CGROUP2_SUPER_MAGIC&&!fstat(fd,&st)&&S_ISDIR(st.st_mode);}
static int mulib_cgroup2_write_(int d,const char*file,const char*s){int fd=openat(d,file,O_WRONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return errno==ENOENT?MULIB_SB_UNSUPPORTED:MULIB_SB_SYSTEM;ssize_t n;size_t len=strlen(s);do{n=write(fd,s,len);}while(n<0&&errno==EINTR);int rc=n==(ssize_t)len?0:MULIB_SB_SYSTEM;if(close(fd)&&!rc)rc=MULIB_SB_SYSTEM;return rc;}
static int mulib_cgroup2_read_(int d,const char*file,char*b,size_t cap){int fd=openat(d,file,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return errno==ENOENT?MULIB_SB_UNSUPPORTED:MULIB_SB_SYSTEM;size_t n=0;int rc=0;while(n<cap-1){ssize_t k=read(fd,b+n,cap-1-n);if(k<0&&errno==EINTR)continue;if(k<0){rc=MULIB_SB_SYSTEM;break;}if(!k)break;n+=(size_t)k;}if(n==cap-1)rc=MULIB_SB_LIMIT;b[n]=0;close(fd);return rc;}
static int mulib_cgroup2_set_(int fd,const char*f,const char*v){int r=mulib_cgroup2_write_(fd,f,v);if(r)return r;char b[256];r=mulib_cgroup2_read_(fd,f,b,sizeof b);if(r)return r;size_t n=strlen(b);while(n&&(b[n-1]=='\n'||b[n-1]==' '))b[--n]=0;return !strcmp(b,v)?0:MULIB_SB_DENIED;}
static int mulib_cgroup2_check_kill_(int fd){int k=openat(fd,"cgroup.kill",O_WRONLY|O_CLOEXEC|O_NOFOLLOW);if(k<0)return errno==ENOENT?MULIB_SB_UNSUPPORTED:MULIB_SB_SYSTEM;return close(k)?MULIB_SB_SYSTEM:0;}
int mulib_cgroup2_create(mulib_cgroup2_t*g,int parent,const char*name,const mulib_cgroup2_limits_t*l){if(!g||g->fd!=-1||g->parent_fd!=-1||!mulib_cgroup2_name_(name)||!l||!l->memory_bytes||!l->pids||l->cpu_period_us<1000||l->cpu_period_us>1000000||l->cpu_quota_us<1000||l->cpu_quota_us>UINT64_C(1000000000)||(l->disable_swap!=0&&l->disable_swap!=1)||!mulib_cgroup2_fs_(parent))return MULIB_SB_INVALID;
    int p=fcntl(parent,F_DUPFD_CLOEXEC,5);if(p<0)return MULIB_SB_SYSTEM;if(mkdirat(p,name,0700)){close(p);return MULIB_SB_SYSTEM;}int fd=openat(p,name,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);int r=MULIB_SB_SYSTEM;char v[128];if(fd<0||!mulib_cgroup2_fs_(fd))goto fail;
    /* Linux rounds memory.max to a page multiple; round down, never up. */
    long page=sysconf(_SC_PAGESIZE);if(page<=0||l->memory_bytes<(uint64_t)page){r=MULIB_SB_INVALID;goto fail;}uint64_t mem=l->memory_bytes/(uint64_t)page*(uint64_t)page;
    snprintf(v,sizeof v,"%" PRIu64,mem);if((r=mulib_cgroup2_set_(fd,"memory.max",v)))goto fail;
    if(l->disable_swap&&(r=mulib_cgroup2_set_(fd,"memory.swap.max","0")))goto fail;
    snprintf(v,sizeof v,"%u",l->pids);if((r=mulib_cgroup2_set_(fd,"pids.max",v)))goto fail;
    snprintf(v,sizeof v,"%" PRIu64 " %" PRIu64,l->cpu_quota_us,l->cpu_period_us);if((r=mulib_cgroup2_set_(fd,"cpu.max",v)))goto fail;
    /* Require cgroup.kill before admitting a sandbox group. */
    if((r=mulib_cgroup2_check_kill_(fd)))goto fail;
    g->fd=fd;g->parent_fd=p;memcpy(g->name,name,strlen(name)+1);return 0;
fail: if(fd>=0)close(fd);if(unlinkat(p,name,AT_REMOVEDIR)){/* Preserve a cleanup handle rather than lose a created directory. */g->parent_fd=p;g->fd=openat(p,name,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);memcpy(g->name,name,strlen(name)+1);return MULIB_SB_STATE;}close(p);return r;}
int mulib_cgroup2_verify(const mulib_cgroup2_t*g,const mulib_cgroup2_limits_t*l){
    if(!g||!l||!mulib_cgroup2_fs_(g->fd)||!l->memory_bytes||!l->pids||l->cpu_period_us<1000||l->cpu_period_us>1000000||l->cpu_quota_us<1000||l->cpu_quota_us>UINT64_C(1000000000)||(l->disable_swap!=0&&l->disable_swap!=1))return MULIB_SB_INVALID;
    char v[128],b[256];long pg=sysconf(_SC_PAGESIZE);if(pg<=0||l->memory_bytes<(uint64_t)pg)return MULIB_SB_INVALID;
    uint64_t m=l->memory_bytes/(uint64_t)pg*(uint64_t)pg;
    snprintf(v,sizeof v,"%" PRIu64 "\n",m);if(mulib_cgroup2_read_(g->fd,"memory.max",b,sizeof b)||strcmp(b,v))return MULIB_SB_DENIED;
    if(l->disable_swap&&(mulib_cgroup2_read_(g->fd,"memory.swap.max",b,sizeof b)||strcmp(b,"0\n")))return MULIB_SB_DENIED;
    snprintf(v,sizeof v,"%u\n",l->pids);if(mulib_cgroup2_read_(g->fd,"pids.max",b,sizeof b)||strcmp(b,v))return MULIB_SB_DENIED;
    snprintf(v,sizeof v,"%" PRIu64 " %" PRIu64 "\n",l->cpu_quota_us,l->cpu_period_us);if(mulib_cgroup2_read_(g->fd,"cpu.max",b,sizeof b)||strcmp(b,v))return MULIB_SB_DENIED;
    return mulib_cgroup2_check_kill_(g->fd);
}
int mulib_cgroup2_attach(const mulib_cgroup2_t*g,long pid){if(!g||!mulib_cgroup2_fs_(g->fd)||pid<=0||pid>INT_MAX)return MULIB_SB_INVALID;char b[64];snprintf(b,sizeof b,"%ld",pid);return mulib_cgroup2_write_(g->fd,"cgroup.procs",b);}
int mulib_cgroup2_populated(const mulib_cgroup2_t*g,int*out){if(!g||!mulib_cgroup2_fs_(g->fd)||!out)return MULIB_SB_INVALID;char b[1024];int r=mulib_cgroup2_read_(g->fd,"cgroup.events",b,sizeof b);if(r)return r;char*s=b;while(*s){if(!strncmp(s,"populated ",10)&&(s[10]=='0'||s[10]=='1')&&(s[11]=='\n'||!s[11])){*out=s[10]-'0';return 0;}char*e=strchr(s,'\n');if(!e)break;s=e+1;}return MULIB_SB_PROTOCOL;}
int mulib_cgroup2_kill(const mulib_cgroup2_t*g){if(!g||!mulib_cgroup2_fs_(g->fd))return MULIB_SB_INVALID;return mulib_cgroup2_write_(g->fd,"cgroup.kill","1");}
int mulib_cgroup2_destroy(mulib_cgroup2_t*g){if(!g)return MULIB_SB_INVALID;if(g->fd==-1&&g->parent_fd==-1)return 0;if(g->parent_fd<0||!mulib_cgroup2_name_(g->name))return MULIB_SB_STATE;if(g->fd>=0){int n,rc=mulib_cgroup2_populated(g,&n);if(rc)return rc;if(n)return MULIB_SB_STATE;}if(unlinkat(g->parent_fd,g->name,AT_REMOVEDIR))return MULIB_SB_SYSTEM;if(g->fd>=0)close(g->fd);close(g->parent_fd);g->fd=g->parent_fd=-1;g->name[0]=0;return 0;}
#else
int mulib_cgroup2_create(mulib_cgroup2_t*g,int f,const char*n,const mulib_cgroup2_limits_t*l){(void)g;(void)f;(void)n;(void)l;return MULIB_SB_UNSUPPORTED;}int mulib_cgroup2_verify(const mulib_cgroup2_t*g,const mulib_cgroup2_limits_t*l){(void)g;(void)l;return MULIB_SB_UNSUPPORTED;}int mulib_cgroup2_attach(const mulib_cgroup2_t*g,long p){(void)g;(void)p;return MULIB_SB_UNSUPPORTED;}int mulib_cgroup2_populated(const mulib_cgroup2_t*g,int*p){(void)g;(void)p;return MULIB_SB_UNSUPPORTED;}int mulib_cgroup2_kill(const mulib_cgroup2_t*g){(void)g;return MULIB_SB_UNSUPPORTED;}int mulib_cgroup2_destroy(mulib_cgroup2_t*g){(void)g;return MULIB_SB_UNSUPPORTED;}
#endif
#endif
