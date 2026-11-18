/** @file mulib-sandbox-process.h
 * Strict Linux broker-only workers. TRUSTED bootstrap ELF must call worker_enter
 * before executing/parsing ANY untrusted code. Not a launcher for an untrusted
 * ELF that can skip the bootstrap. Single-threaded supervisor, no shell/PATH.
 * Define MULIB_IMPL_SANDBOX_PROCESS and dependency implementation switches once.
 */
#ifndef MULIB_SANDBOX_PROCESS_H
#define MULIB_SANDBOX_PROCESS_H
#include "mulib-sandbox-profile.h"
#include "mulib-seccomp.h"
#include "mulib-ipc-framed.h"
#include "mulib-landlock.h"
#include "mulib-namespace.h"
#include "mulib-cgroup2.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_sandbox_launch {
    int trusted_elf_fd; /* borrowed readable descriptor, identity approved by host */
    mulib_sandbox_profile_t profile;
    int drop_identity; uint32_t uid,gid; /* explicit nonzero IDs for root host */
    const mulib_landlock_rule_t *landlock_rules;size_t landlock_count;
    mulib_namespace_spec_t namespace_spec;
    mulib_cgroup2_t *group; /* borrowed empty, dedicated verified group if required */
} mulib_sandbox_launch_t;
typedef struct mulib_sandbox_process {long pid;int fd;int64_t deadline;mulib_ipc_framed_t ipc;mulib_cgroup2_t *group;uint32_t enforced;} mulib_sandbox_process_t;
#define MULIB_SANDBOX_PROCESS_INIT {0,-1,0,{-1,0,0},NULL,0}
typedef struct mulib_sandbox_exit {int exited,exit_code,signal_number;} mulib_sandbox_exit_t;
/** Start trusted ELF via execveat, release after group attachment, require
 * bootstrap handshake plus independent /proc credential/filter inspection.
 * If cleanup fails the output owns the remaining child/connection. */
int mulib_sandbox_process_start(mulib_sandbox_process_t *out,const mulib_sandbox_launch_t *spec);
/** Trusted worker only: reads setup fd 4, closes it, applies rlimits and filter,
 * emits ready via fd 3. On failure terminates with _exit(125), never runs plugin.
 * Call before ANY untrusted code. Worker must wait for first request. */
int mulib_sandbox_worker_enter(mulib_ipc_framed_t *ipc,mulib_sandbox_profile_t *profile);
int mulib_sandbox_process_send(mulib_sandbox_process_t *p,const void *bytes,size_t len);
int mulib_sandbox_process_receive(mulib_sandbox_process_t *p,void *bytes,size_t cap,size_t *len);
/** Drive periodically even without IPC: kill/reap on expired wall deadline. */
int mulib_sandbox_process_tick(mulib_sandbox_process_t *p,int64_t cleanup_deadline,mulib_sandbox_exit_t *status);
/** Nonblocking reap: AGAIN while live. On reap closes IPC and clears pid.
 * If configured, call stop afterward to verify and release the cgroup borrow. */
int mulib_sandbox_process_poll(mulib_sandbox_process_t *p,mulib_sandbox_exit_t *status);
/** Kill (including cgroup if configured) and reap by absolute deadline; retains
 * handle on timeout. Never signal a reaped pid. */
int mulib_sandbox_process_stop(mulib_sandbox_process_t *p,int64_t deadline,mulib_sandbox_exit_t *status);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SANDBOX_PROCESS) && !defined(MULIB_SANDBOX_PROCESS_IMPLEMENTED)
#define MULIB_SANDBOX_PROCESS_IMPLEMENTED
#if defined(__linux__) && defined(__LP64__) && (defined(__x86_64__) || defined(__aarch64__))
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <grp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/personality.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <linux/capability.h>
#include <linux/magic.h>
/* This launch-only wire format is private to identical host/worker builds. */
typedef struct mulib_sb_setup_ {uint64_t marker;long parent;mulib_sandbox_profile_t profile;} mulib_sb_setup_;
#define MULIB_SB_SETUP_MARKER UINT64_C(0x4d554c4942534231)
static int mulib_sb_limit_(int resource,uint64_t n){rlim_t v=(rlim_t)n;if((uint64_t)v!=n||v==RLIM_INFINITY)return 0;struct rlimit r={v,v};return !setrlimit(resource,&r)&&!getrlimit(resource,&r)&&r.rlim_cur==v&&r.rlim_max==v;}
static int mulib_sb_parent_(long parent){return parent>0&&!prctl(PR_SET_PDEATHSIG,SIGKILL,0,0,0)&&(long)getppid()==parent;}
static int mulib_sb_credentials_(void){struct __user_cap_header_struct h={_LINUX_CAPABILITY_VERSION_3,0};struct __user_cap_data_struct d[2]={{0}};
    /* Bounding set may remain on unprivileged caller; no_new_privs plus
       zero effective/permitted/inheritable/ambient and no exec are mandatory. */
    if(prctl(PR_CAP_AMBIENT,PR_CAP_AMBIENT_CLEAR_ALL,0,0,0)||syscall(SYS_capset,&h,d)||prctl(PR_SET_DUMPABLE,0,0,0,0)||prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0))return 0;
    return 1;}
static int mulib_sb_read_exact_(int fd,void*ptr,size_t n){unsigned char*p=ptr;while(n){ssize_t r=read(fd,p,n);if(r<0&&errno==EINTR)continue;if(r<=0)return 0;p+=(size_t)r;n-=(size_t)r;}return 1;}
static int mulib_sb_send_setup_(int fd,const void*ptr,size_t n){const unsigned char*p=ptr;while(n){ssize_t r=send(fd,p,n,MSG_NOSIGNAL|MSG_DONTWAIT);if(r<0&&errno==EINTR)continue;if(r<=0)return 0;p+=(size_t)r;n-=(size_t)r;}return 1;}
static int mulib_sb_dup_high_(int fd){return fcntl(fd,F_DUPFD_CLOEXEC,10);}
static int mulib_sb_closerange_(unsigned first){
#ifdef SYS_close_range
    return syscall(SYS_close_range,first,UINT_MAX,0)==0;
#else
    (void)first;return 0;
#endif
}
static void mulib_sb_close_(int fd){if(fd>=0)(void)close(fd);}
static int mulib_sb_verify_(long pid,int before,const mulib_sandbox_launch_t*s){char path[80],line[256];snprintf(path,sizeof path,"/proc/%ld/status",pid);FILE*f=fopen(path,"r");if(!f)return -1;struct statfs fs;if(fstatfs(fileno(f),&fs)||fs.f_type!=PROC_SUPER_MAGIC){fclose(f);return -1;}int sec=-1,nn=-1,filters=-1,cap=0,uid_ok=0,gid_ok=0,groups_empty=0;unsigned long long v;char state=0;
    unsigned long uid=s&&s->drop_identity?s->uid:(unsigned long)geteuid();
    unsigned long gid=s&&s->drop_identity?s->gid:(unsigned long)getegid();
    while(fgets(line,sizeof line,f)){int i;if(sscanf(line,"Seccomp: %d",&i)==1)sec=i;else if(sscanf(line,"Seccomp_filters: %d",&i)==1)filters=i;else if(sscanf(line,"NoNewPrivs: %d",&i)==1)nn=i;
    else if(!strncmp(line,"State:",6)){(void)sscanf(line+6," %c",&state);}
    else if(!strncmp(line,"Uid:",4)||!strncmp(line,"Gid:",4)){unsigned long ids[4];if(sscanf(line+4,"%lu %lu %lu %lu",ids,ids+1,ids+2,ids+3)==4){int is_uid=line[0]=='U';unsigned long expected=is_uid?uid:gid;
        int ok=ids[0]==expected&&ids[1]==expected&&ids[2]==expected&&ids[3]==expected;if(is_uid)uid_ok=ok;else gid_ok=ok;}}
    else if(!strncmp(line,"Groups:",7)){groups_empty=line[7+strspn(line+7," \t\r")] == '\n';}
    else if(sscanf(line,"CapEff: %llx",&v)==1){if(!v)cap|=1;}
    else if(sscanf(line,"CapPrm: %llx",&v)==1){if(!v)cap|=2;}
    else if(sscanf(line,"CapInh: %llx",&v)==1){if(!v)cap|=4;}
    else if(sscanf(line,"CapAmb: %llx",&v)==1){if(!v)cap|=8;}}
    int error=ferror(f);fclose(f);if(error)return -1;if(before<0)return filters>=0?filters:(sec==0?0:-1);
    return state&&state!='Z'&&state!='X'&&sec==2&&nn==1&&cap==15&&uid_ok&&gid_ok&&(!s->drop_identity||groups_empty)&&filters>0&&filters-1==before?0:-1;}
static int mulib_sb_verify_group_(const mulib_cgroup2_t*g,long pid){
    /* The launcher requires an exclusive group with pids.max=1. Confirm the
       actual membership as well as the parent-side migration's return code. */
    int fd=openat(g->fd,"cgroup.procs",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return MULIB_SB_SYSTEM;
    char expected[64],got[64];int len=snprintf(expected,sizeof expected,"%ld\n",pid);
    int ok=len>0&&(size_t)len<sizeof got&&mulib_sb_read_exact_(fd,got,(size_t)len)&&!memcmp(got,expected,(size_t)len);
    char extra;ssize_t n;do{n=read(fd,&extra,1);}while(n<0&&errno==EINTR);
    close(fd);return ok&&n==0?0:MULIB_SB_DENIED;
}
int mulib_sandbox_worker_enter(mulib_ipc_framed_t*ipc,mulib_sandbox_profile_t*out){mulib_sb_setup_ s;mulib_seccomp_policy_t policy;int64_t deadline;unsigned char msg[8]={'S','B','0','1',0,0,0,0};
    if(!ipc||!out||!mulib_sb_read_exact_(MULIB_SANDBOX_SETUP_FD,&s,sizeof s)||s.marker!=MULIB_SB_SETUP_MARKER||mulib_sandbox_profile_validate(&s.profile)||!mulib_sb_parent_(s.parent))_exit(125);
    if(close(MULIB_SANDBOX_SETUP_FD)||!mulib_sb_closerange_(5)||mulib_ipc_framed_init(ipc,MULIB_SANDBOX_IPC_FD,s.profile.max_frame_bytes)||mulib_seccomp_worker(&policy,MULIB_SANDBOX_IPC_FD,0)||mulib_ipc_deadline(s.profile.startup_ms,&deadline))_exit(125);
    if(!mulib_sb_limit_(RLIMIT_AS,s.profile.memory_bytes)||!mulib_sb_limit_(RLIMIT_STACK,s.profile.stack_bytes)||!mulib_sb_limit_(RLIMIT_CPU,s.profile.cpu_seconds)||!mulib_sb_limit_(RLIMIT_CORE,0)||!mulib_sb_limit_(RLIMIT_FSIZE,0)||!mulib_sb_limit_(RLIMIT_NPROC,0)||!mulib_sb_limit_(RLIMIT_MEMLOCK,0)||!mulib_sb_limit_(RLIMIT_NOFILE,s.profile.max_open_files)||!mulib_sb_credentials_())_exit(125);
    int pers=personality(0xffffffffUL);
    if(pers<0||(pers&READ_IMPLIES_EXEC))_exit(125);
    *out=s.profile;msg[7]=(unsigned char)s.profile.required;
    if(mulib_seccomp_install(&policy)||mulib_ipc_send(ipc,msg,sizeof msg,deadline))_exit(125);
    return 0;}
static int mulib_sb_drop_identity_(const mulib_sandbox_launch_t*s){
    return !setgroups(0,NULL)&&!setresgid((gid_t)s->gid,(gid_t)s->gid,(gid_t)s->gid)&&!setresuid((uid_t)s->uid,(uid_t)s->uid,(uid_t)s->uid);
}
static void mulib_sb_child_(const mulib_sandbox_launch_t*s,int sock,int setup,int elf,int devnull,long parent){unsigned char go;/* Child runs only trusted code. Single-thread supervisor required. */
    if(!mulib_sb_parent_(parent)||!mulib_sb_read_exact_(setup,&go,1)||go!=0x47)_exit(125);
    /* A user namespace maps the current host identity. Drop root first, so
       a namespace root never maps to host root. Other namespaces may need host
       privileges and therefore precede the identity drop. */
    int userns=!!(s->namespace_spec.flags&MULIB_SB_NS_USER);
    if(s->drop_identity&&userns&&(!mulib_sb_drop_identity_(s)||!mulib_sb_parent_(parent)))_exit(125);
    if(s->profile.required&MULIB_SB_ENFORCE_NAMESPACE){uint32_t applied;if(mulib_namespace_enter(&s->namespace_spec,&applied)||applied!=s->profile.namespace_flags)_exit(125);}
    if(s->drop_identity&&!userns&&!mulib_sb_drop_identity_(s))_exit(125);
    if(!mulib_sb_parent_(parent)||chdir("/")||prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0))_exit(125);
    if((s->profile.required&MULIB_SB_ENFORCE_LANDLOCK)&&mulib_landlock_restrict(s->landlock_rules,s->landlock_count,s->profile.landlock_min_abi))_exit(125);
    for(int i=0;i<3;i++)if(dup3(devnull,i,0)<0)_exit(125);
    if(dup3(sock,3,0)<0||dup3(setup,4,0)<0||dup3(elf,5,O_CLOEXEC)<0||!mulib_sb_closerange_(6))_exit(125);
    char *const argv[]={ (char*)"mulib-sandbox-worker", (char*)"--mulib-worker", NULL};char *const env[]={ (char*)"LC_ALL=C", NULL};
#ifdef SYS_execveat
    syscall(SYS_execveat,5,"",argv,env,AT_EMPTY_PATH);
#else
    (void)argv;(void)env;
#endif
    _exit(125);}
static void mulib_sb_clear_(mulib_sandbox_process_t*p){mulib_sb_close_(p->fd);p->fd=-1;p->pid=0;p->ipc.fd=-1;p->ipc.poisoned=1;p->deadline=0;p->enforced=0;/* group remains borrowed; stop may check it */}
int mulib_sandbox_process_poll(mulib_sandbox_process_t*p,mulib_sandbox_exit_t*out){if(!p||p->pid<=0||!out)return MULIB_SB_INVALID;int st;pid_t n;do{n=waitpid((pid_t)p->pid,&st,WNOHANG);}while(n<0&&errno==EINTR);if(!n)return MULIB_SB_AGAIN;if(n<0){if(errno==ECHILD)mulib_sb_clear_(p);return MULIB_SB_SYSTEM;}mulib_sandbox_exit_t v={0,0,0};if(WIFEXITED(st)){v.exited=1;v.exit_code=WEXITSTATUS(st);}else if(WIFSIGNALED(st))v.signal_number=WTERMSIG(st);else return MULIB_SB_AGAIN;*out=v;mulib_sb_clear_(p);return 0;}
int mulib_sandbox_process_stop(mulib_sandbox_process_t*p,int64_t end,mulib_sandbox_exit_t*out){if(!p||!out||end<0)return MULIB_SB_INVALID;int r=0;
    /* Check that the PID is still ours before signaling. Under the exclusive
       wait contract it then remains reserved even if the child exits. */
    if(p->pid>0){int q=mulib_sandbox_process_poll(p,out);if(q!=MULIB_SB_AGAIN&&q){if(p->pid>0)return q;r=q;}}
    if(p->group){int n,q=mulib_cgroup2_populated(p->group,&n);if(!q&&n)q=mulib_cgroup2_kill(p->group);if(q)r=q;}
    if(p->pid>0&&kill((pid_t)p->pid,SIGKILL)&&errno!=ESRCH)return MULIB_SB_SYSTEM;
    for(;;){if(p->pid>0){int q=mulib_sandbox_process_poll(p,out);if(q!=MULIB_SB_AGAIN&&q){if(p->pid>0)return q;r=q;}}
        int populated=0,group_status=0;if(p->group){group_status=mulib_cgroup2_populated(p->group,&populated);if(group_status)r=group_status;}
        if(!p->pid&&group_status)return r;
        if(!p->pid&&!populated){p->group=NULL;return r;}
        int64_t now=mulib_ipc_now_ms();if(now<0)return MULIB_SB_SYSTEM;
        if(now>=end)return r?r:MULIB_SB_TIMEOUT;
        int64_t ms=end-now;if(ms>5)ms=5;struct timespec t={0,(long)ms*1000000L};nanosleep(&t,NULL);}}
int mulib_sandbox_process_start(mulib_sandbox_process_t*out,const mulib_sandbox_launch_t*s){if(!out||out->pid||out->fd!=-1||out->group||!s||mulib_sandbox_profile_validate(&s->profile)||s->trusted_elf_fd<0|| (s->drop_identity!=0&&s->drop_identity!=1)||s->uid==UINT32_MAX||s->gid==UINT32_MAX||
    (!!s->group!=!!(s->profile.required&MULIB_SB_ENFORCE_CGROUP))||s->namespace_spec.flags!=s->profile.namespace_flags||
    ((s->profile.required&MULIB_SB_ENFORCE_LANDLOCK)==0&&(s->landlock_rules||s->landlock_count))||(!s->landlock_rules&&s->landlock_count)||s->landlock_count>4096||
    (s->namespace_spec.flags&MULIB_SB_NS_PID)||
     (s->drop_identity&&(!s->uid||!s->gid))||(!s->drop_identity&&(!geteuid()||!getegid()||getuid()!=geteuid()||getgid()!=getegid())))return MULIB_SB_INVALID;
#ifndef SYS_execveat
    return MULIB_SB_UNSUPPORTED;
#endif
    if(!mulib_seccomp_supported())return MULIB_SB_UNSUPPORTED;
    if(s->profile.required&MULIB_SB_ENFORCE_LANDLOCK){int a=mulib_landlock_abi();if(a<0)return a;if((unsigned)a<s->profile.landlock_min_abi)return MULIB_SB_UNSUPPORTED;}
    if(s->group){int n,r=mulib_cgroup2_populated(s->group,&n);if(r)return r;if(n)return MULIB_SB_STATE;
        mulib_cgroup2_limits_t l={s->profile.cgroup_memory_bytes,s->profile.cpu_quota_us,s->profile.cpu_period_us,1,1};
        if((r=mulib_cgroup2_verify(s->group,&l)))return r;}
    /* Exclusive child ownership: no signal handler or auto-reap may consume it. */
    struct sigaction sa;if(sigaction(SIGCHLD,NULL,&sa)||sa.sa_handler!=SIG_DFL||(sa.sa_flags&SA_NOCLDWAIT))return MULIB_SB_STATE;
    struct stat st;unsigned char elf_magic[4];if(fstat(s->trusted_elf_fd,&st)||!S_ISREG(st.st_mode)||(st.st_mode&(S_ISUID|S_ISGID|S_IWGRP|S_IWOTH))||pread(s->trusted_elf_fd,elf_magic,4,0)!=4||memcmp(elf_magic,"\177ELF",4))return MULIB_SB_INVALID;
    long parent=(long)getpid();int base=mulib_sb_verify_(parent,-1,NULL);if(base<0)return MULIB_SB_UNSUPPORTED;
    int pair[2]={-1,-1},set[2]={-1,-1},a=-1,b=-1,e=-1,dn=-1,d=-1;int r=MULIB_SB_SYSTEM;int64_t startup;
    if(mulib_ipc_deadline(s->profile.startup_ms,&startup))return MULIB_SB_INVALID;
    if(socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,pair)||socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,set))goto done;
    a=mulib_sb_dup_high_(pair[1]);b=mulib_sb_dup_high_(set[1]);e=mulib_sb_dup_high_(s->trusted_elf_fd);dn=open("/dev/null",O_RDWR|O_CLOEXEC|O_NOFOLLOW);if(dn>=0)d=mulib_sb_dup_high_(dn);if(a<0||b<0||e<0||d<0)goto done;
    pid_t pid=fork();if(pid<0)goto done;if(!pid){/* Close parent's copies before barrier. */mulib_sb_close_(pair[0]);mulib_sb_close_(set[0]);mulib_sb_child_(s,a,b,e,d,parent);_exit(125);}
    out->pid=pid;out->fd=pair[0];pair[0]=-1;out->group=s->group;mulib_sb_close_(pair[1]);pair[1]=-1;mulib_sb_close_(set[1]);set[1]=-1;mulib_sb_close_(a);a=-1;mulib_sb_close_(b);b=-1;
    if(s->group&&((r=mulib_cgroup2_attach(s->group,pid))||(r=mulib_sb_verify_group_(s->group,pid))))goto fail;
    {unsigned char go=0x47;mulib_sb_setup_ cfg={MULIB_SB_SETUP_MARKER,parent,s->profile};if(!mulib_sb_send_setup_(set[0],&go,1)||!mulib_sb_send_setup_(set[0],&cfg,sizeof cfg)){r=MULIB_SB_SYSTEM;goto fail;}}
    mulib_sb_close_(set[0]);set[0]=-1;
    if((r=mulib_ipc_framed_init(&out->ipc,out->fd,s->profile.max_frame_bytes)))goto fail;
    {unsigned char ready[8],expected[8]={'S','B','0','1',0,0,0,(unsigned char)s->profile.required};size_t n=0;r=mulib_ipc_receive(&out->ipc,ready,sizeof ready,&n,startup);if(r)goto fail;if(n!=8||memcmp(ready,expected,8)||mulib_sb_verify_(pid,base,s)){r=MULIB_SB_DENIED;goto fail;}}
    if(s->group&&(r=mulib_sb_verify_group_(s->group,pid)))goto fail;
    {int64_t now=mulib_ipc_now_ms();if(now<0){r=MULIB_SB_SYSTEM;goto fail;}if(now>=startup){r=MULIB_SB_TIMEOUT;goto fail;}}
    if((r=mulib_ipc_deadline(s->profile.wall_ms,&out->deadline)))goto fail;
    out->enforced=s->profile.required;goto done;
fail: {int64_t end;mulib_sandbox_exit_t status={0};if(!mulib_ipc_deadline(3000,&end)){int q=mulib_sandbox_process_stop(out,end,&status);if(q)r=q;}}
done:mulib_sb_close_(a);mulib_sb_close_(b);mulib_sb_close_(e);mulib_sb_close_(d);mulib_sb_close_(dn);mulib_sb_close_(pair[0]);mulib_sb_close_(pair[1]);mulib_sb_close_(set[0]);mulib_sb_close_(set[1]);return r;}
int mulib_sandbox_process_tick(mulib_sandbox_process_t*p,int64_t end,mulib_sandbox_exit_t*out){if(!p||!out||!p->enforced||p->pid<=0||end<0)return MULIB_SB_STATE;int64_t now=mulib_ipc_now_ms();if(now<0)return MULIB_SB_SYSTEM;if(now>=p->deadline){int r=mulib_sandbox_process_stop(p,end,out);return r?r:MULIB_SB_TIMEOUT;}return mulib_sandbox_process_poll(p,out);}
int mulib_sandbox_process_send(mulib_sandbox_process_t*p,const void*b,size_t n){if(!p||!p->enforced||p->pid<=0)return MULIB_SB_STATE;return mulib_ipc_send(&p->ipc,b,n,p->deadline);}
int mulib_sandbox_process_receive(mulib_sandbox_process_t*p,void*b,size_t cap,size_t*n){if(!p||!p->enforced||p->pid<=0)return MULIB_SB_STATE;return mulib_ipc_receive(&p->ipc,b,cap,n,p->deadline);}
#else
int mulib_sandbox_process_tick(mulib_sandbox_process_t*p,int64_t e,mulib_sandbox_exit_t*s){(void)p;(void)e;(void)s;return MULIB_SB_UNSUPPORTED;}int mulib_sandbox_process_start(mulib_sandbox_process_t*p,const mulib_sandbox_launch_t*s){(void)p;(void)s;return MULIB_SB_UNSUPPORTED;}int mulib_sandbox_worker_enter(mulib_ipc_framed_t*p,mulib_sandbox_profile_t*q){(void)p;(void)q;return MULIB_SB_UNSUPPORTED;}int mulib_sandbox_process_send(mulib_sandbox_process_t*p,const void*b,size_t n){(void)p;(void)b;(void)n;return MULIB_SB_UNSUPPORTED;}int mulib_sandbox_process_receive(mulib_sandbox_process_t*p,void*b,size_t c,size_t*n){(void)p;(void)b;(void)c;(void)n;return MULIB_SB_UNSUPPORTED;}int mulib_sandbox_process_poll(mulib_sandbox_process_t*p,mulib_sandbox_exit_t*s){(void)p;(void)s;return MULIB_SB_UNSUPPORTED;}int mulib_sandbox_process_stop(mulib_sandbox_process_t*p,int64_t e,mulib_sandbox_exit_t*s){(void)p;(void)e;(void)s;return MULIB_SB_UNSUPPORTED;}
#endif
#endif
