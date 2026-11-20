#include "mulib-sandbox-process.h"
#include "mulib-capbroker.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <sys/mman.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s (errno=%d)\n",__FILE__,__LINE__,#x,errno);exit(1);}}while(0)
static int64_t end(uint64_t ms){int64_t e;CHECK(!mulib_ipc_deadline(ms,&e));return e;}
static void reap_status(pid_t p,int code){int st;CHECK(waitpid(p,&st,0)==p);CHECK(WIFEXITED(st)&&WEXITSTATUS(st)==code);}
static void profile_test(void){mulib_sandbox_profile_t p=mulib_sandbox_profile_default();CHECK(!mulib_sandbox_profile_validate(&p));p.required=0;CHECK(mulib_sandbox_profile_validate(&p)<0);p=mulib_sandbox_profile_default();p.namespace_flags=MULIB_SB_NS_USER;CHECK(mulib_sandbox_profile_validate(&p)<0);p.required|=MULIB_SB_ENFORCE_NAMESPACE;CHECK(!mulib_sandbox_profile_validate(&p));p.max_frame_bytes=7;CHECK(mulib_sandbox_profile_validate(&p)<0);puts("profile: ok");}
static void ipc_test(void){int f[2];mulib_ipc_framed_t a,b;unsigned char data[]={0,255,3},buf[8];size_t n=99;CHECK(!socketpair(AF_UNIX,SOCK_STREAM,0,f));CHECK(!mulib_ipc_framed_init(&a,f[0],8));CHECK(!mulib_ipc_framed_init(&b,f[1],8));CHECK(!mulib_ipc_send(&a,data,3,end(1000)));CHECK(!mulib_ipc_receive(&b,buf,sizeof buf,&n,end(1000))&&n==3&&!memcmp(data,buf,3));CHECK(!mulib_ipc_send(&a,NULL,0,end(1000)));CHECK(!mulib_ipc_receive(&b,NULL,0,&n,end(1000))&&!n);CHECK(mulib_ipc_send(&a,buf,9,end(1000))==MULIB_SB_LIMIT&&!a.poisoned);n=99;CHECK(mulib_ipc_receive(&b,buf,sizeof buf,&n,end(20))==MULIB_SB_TIMEOUT&&b.poisoned&&n==99);CHECK(mulib_ipc_receive(&b,buf,sizeof buf,&n,end(20))==MULIB_SB_STATE);close(f[0]);close(f[1]);
    CHECK(!socketpair(AF_UNIX,SOCK_STREAM,0,f));CHECK(!mulib_ipc_framed_init(&b,f[1],8));unsigned char header[4]={0,0,0,9};CHECK(write(f[0],header,4)==4);CHECK(mulib_ipc_receive(&b,buf,8,&n,end(1000))==MULIB_SB_LIMIT&&b.poisoned);close(f[0]);close(f[1]);
    CHECK(!socketpair(AF_UNIX,SOCK_STREAM,0,f));CHECK(!mulib_ipc_framed_init(&a,f[0],8));close(f[1]);CHECK(mulib_ipc_send(&a,data,3,end(1000))<0);close(f[0]);
    const unsigned char frame[]={0,0,0,3,1,2};
    const size_t prefixes[]={0,2,4,6};
    for(size_t i=0;i<sizeof prefixes/sizeof prefixes[0];i++){
        CHECK(!socketpair(AF_UNIX,SOCK_STREAM,0,f));CHECK(!mulib_ipc_framed_init(&b,f[1],8));
        CHECK(write(f[0],frame,prefixes[i])==(ssize_t)prefixes[i]);close(f[0]);n=99;
        CHECK(mulib_ipc_receive(&b,buf,8,&n,end(1000))==(i?MULIB_SB_PROTOCOL:MULIB_SB_EOF)&&n==99&&b.poisoned);close(f[1]);}
    CHECK(!socketpair(AF_UNIX,SOCK_STREAM,0,f));CHECK(!mulib_ipc_framed_init(&b,f[1],8));
    CHECK(write(f[0],frame,6)==6);CHECK(mulib_ipc_receive(&b,buf,8,&n,0)==MULIB_SB_TIMEOUT&&n==99&&b.poisoned);
    close(f[0]);close(f[1]);puts("IPC framing, binary, zero, timeout, truncation, oversize, peer close: ok");}
static volatile sig_atomic_t interruptions;
static void on_signal(int signum){(void)signum;interruptions++;}
static void interrupt_test(void){
    struct sigaction act={0},old;act.sa_handler=on_signal;CHECK(!sigemptyset(&act.sa_mask)&&!sigaction(SIGUSR1,&act,&old));
    for(int reply=0;reply<2;reply++){
        int f[2];mulib_ipc_framed_t ipc;CHECK(!socketpair(AF_UNIX,SOCK_STREAM,0,f));CHECK(!mulib_ipc_framed_init(&ipc,f[0],8));
        pid_t pid=fork();CHECK(pid>=0);
        if(!pid){
            close(f[0]);struct timespec delay={0,10000000L};
            for(int i=0;i<10;i++){if(nanosleep(&delay,NULL)||kill(getppid(),SIGUSR1))_exit(2);}
            if(reply&&write(f[1],"\0\0\0\1x",5)!=5)_exit(3);
            if(!reply){delay.tv_nsec=250000000L;if(nanosleep(&delay,NULL))_exit(4);}
            _exit(0);
        }
        close(f[1]);char b=0;size_t n=99;interruptions=0;
        int r=mulib_ipc_receive(&ipc,&b,1,&n,end(reply?2000:70));
        CHECK(interruptions>0);
        if(reply)CHECK(!r&&n==1&&b=='x'&&!ipc.poisoned);
        else CHECK(r==MULIB_SB_TIMEOUT&&n==99&&ipc.poisoned);
        close(f[0]);int st;pid_t got;do{got=waitpid(pid,&st,0);}while(got<0&&errno==EINTR);
        CHECK(got==pid&&WIFEXITED(st)&&!WEXITSTATUS(st));
    }
    CHECK(!sigaction(SIGUSR1,&old,NULL));puts("IPC: interrupted poll resumes; repeated signals do not reset the deadline");
}
typedef struct txn{int stage,committed,aborted,audited,uncertain;} txn;
static int begin(void*c,const char*s){txn*t=c;CHECK(!strcmp(s,"test-worker"));t->stage=0;return 0;}
static int commit(void*c){txn*t=c;t->committed+=t->stage;t->stage=0;return t->uncertain?MULIB_SB_UNCERTAIN:0;}
static void abort_tx(void*c){txn*t=c;t->aborted++;t->stage=0;}
static void audit(void*c,const char*s,const char*o,int r){txn*t=c;(void)s;(void)o;(void)r;t->audited++;}
static int op(void*c,const char*s,const void*r,size_t n,void*out,size_t cap,size_t*len){txn*t=c;CHECK(!strcmp(s,"test-worker"));if(n>cap)return MULIB_SB_LIMIT;memcpy(out,r,n);*len=n;t->stage++;return 0;}
static int bad_reply(void*c,const char*s,const void*r,size_t n,void*out,size_t cap,size_t*len){(void)c;(void)s;(void)r;(void)n;(void)out;*len=cap+1;return 0;}
static void broker_test(void){txn t={0};mulib_capbroker_t b;mulib_sandbox_profile_t p=mulib_sandbox_profile_default();p.max_broker_requests=2;p.max_frame_bytes=8;p.max_broker_bytes=16;const char*grants[]={"proposal.write"};const char*requested[]={"proposal.write","approval.write"};const char*effective[2];size_t count=99;CHECK(!mulib_capbroker_intersect(requested,2,grants,1,effective,2,&count)&&count==1&&effective[0]==grants[0]);CHECK(mulib_capbroker_intersect(requested,2,grants,1,NULL,0,&count)==MULIB_SB_LIMIT&&count==1);mulib_capbroker_op_t ops[]={{"submit","proposal.write",op,&t},{"review","approval.write",op,&t},{"bad","proposal.write",bad_reply,&t}};mulib_capbroker_hooks_t h={begin,commit,abort_tx,audit,&t};char buf[16];size_t n;
    CHECK(!mulib_capbroker_init(&b,"test-worker",ops,3,grants,1,&h,&p));CHECK(!mulib_capbroker_begin(&b));CHECK(!mulib_capbroker_call(&b,"submit","a",1,buf,16,&n)&&n==1);CHECK(mulib_capbroker_call(&b,"review","x",1,buf,16,&n)==MULIB_SB_DENIED);CHECK(mulib_capbroker_call(&b,"submit","x",1,buf,16,&n)==MULIB_SB_DENIED);CHECK(mulib_capbroker_finish(&b,1)==MULIB_SB_DENIED&&t.aborted==1&&!t.committed);
    CHECK(!mulib_capbroker_begin(&b));CHECK(!mulib_capbroker_call(&b,"submit","a",1,buf,16,&n));CHECK(!mulib_capbroker_finish(&b,1)&&t.committed==1);CHECK(mulib_capbroker_finish(&b,1)==MULIB_SB_STATE);
    CHECK(!mulib_capbroker_begin(&b));CHECK(!mulib_capbroker_call(&b,"submit","12345678",8,buf,16,&n)&&n==8);CHECK(mulib_capbroker_call(&b,"submit","x",1,buf,16,&n)==MULIB_SB_LIMIT);CHECK(mulib_capbroker_finish(&b,1)==MULIB_SB_DENIED);
    CHECK(!mulib_capbroker_begin(&b));CHECK(mulib_capbroker_call(&b,"bad","x",1,buf,16,&n)==MULIB_SB_PROTOCOL);CHECK(mulib_capbroker_finish(&b,1)==MULIB_SB_DENIED);t.uncertain=1;int aborts=t.aborted;CHECK(!mulib_capbroker_begin(&b));CHECK(!mulib_capbroker_call(&b,"submit","x",1,buf,16,&n));CHECK(mulib_capbroker_finish(&b,1)==MULIB_SB_UNCERTAIN&&t.aborted==aborts);puts("broker authorization, sticky deny, quotas, rollback, uncertain commit: ok");}
static void seccomp_test(void){CHECK(mulib_seccomp_supported());mulib_seccomp_policy_t p;CHECK(!mulib_seccomp_init(&p,EPERM));mulib_seccomp_check_t c={6,1,0};CHECK(mulib_seccomp_allow(&p,SYS_read,&c,1)==MULIB_SB_INVALID);c.argument=0;c.mask=1;c.value=2;CHECK(mulib_seccomp_allow(&p,SYS_read,&c,1)==MULIB_SB_INVALID);CHECK(mulib_seccomp_allow(&p,0x40000000,NULL,0)==MULIB_SB_INVALID);pid_t pid=fork();CHECK(pid>=0);if(!pid){if(mulib_seccomp_worker(&p,3,EPERM)||mulib_seccomp_install(&p))_exit(2);errno=0;long fd=syscall(SYS_openat,AT_FDCWD,"/etc/passwd",O_RDONLY,0);if(fd!=-1||errno!=EPERM)_exit(3);errno=0;long w=syscall(SYS_write,1,"x",1);if(w!=-1||errno!=EPERM)_exit(4);_exit(0);}reap_status(pid,0);puts("seccomp: validated custom rules and real errno denial");}
static void landlock_test(void){int abi=mulib_landlock_abi();if(abi<3){printf("SKIP landlock positive: ABI=%d\n",abi);return;}char dir[]="/tmp/mulib-landlock-XXXXXX";CHECK(mkdtemp(dir));char file[256];snprintf(file,sizeof file,"%s/file",dir);int f=open(file,O_CREAT|O_WRONLY|O_EXCL,0600);CHECK(f>=0&&write(f,"x",1)==1);close(f);int path=open(dir,O_PATH|O_DIRECTORY|O_CLOEXEC);CHECK(path>=0);pid_t p=fork();CHECK(p>=0);if(!p){mulib_landlock_rule_t r={path,MULIB_LANDLOCK_READ_FILE|MULIB_LANDLOCK_READ_DIR};if(mulib_landlock_restrict(&r,1,3))_exit(2);close(path);int fd=open(file,O_RDONLY);if(fd<0)_exit(3);close(fd);errno=0;if(open("/etc/passwd",O_RDONLY)>=0||errno!=EACCES)_exit(4);errno=0;if(open(file,O_WRONLY|O_TRUNC)>=0||errno!=EACCES)_exit(5);_exit(0);}reap_status(p,0);close(path);unlink(file);rmdir(dir);printf("landlock: actual read allow, external read/write deny (ABI %d)\n",abi);}
static void namespace_test(void){mulib_namespace_spec_t s={0,0,0};uint32_t a=99;CHECK(mulib_namespace_enter(&s,&a)==MULIB_SB_INVALID);pid_t pid=fork();CHECK(pid>=0);if(!pid){char before[128],after[128];ssize_t n=readlink("/proc/self/ns/net",before,sizeof before);if(n<0)_exit(2);s.flags=MULIB_SB_NS_USER|MULIB_SB_NS_NETWORK;int rc=mulib_namespace_enter(&s,&a);if(rc){if(errno==EPERM||errno==EACCES||errno==EINVAL||errno==ENOSYS)_exit(77);_exit(3);}ssize_t m=readlink("/proc/self/ns/net",after,sizeof after);if(m<=0||a!=s.flags||(n==m&&!memcmp(before,after,(size_t)n)))_exit(4);_exit(0);}int st;CHECK(waitpid(pid,&st,0)==pid&&WIFEXITED(st));if(WEXITSTATUS(st)==77)puts("SKIP namespace positive: host disallows user/network unshare");else{CHECK(!WEXITSTATUS(st));puts("namespace: actual independent network namespace");}}
static void namespace_mount_pid_test(void){
    const uint32_t checks[]={MULIB_SB_NS_MOUNT,MULIB_SB_NS_PID};
    for(size_t i=0;i<sizeof checks/sizeof checks[0];i++){
        pid_t child=fork();CHECK(child>=0);
        if(!child){
            pid_t original=getpid();
            mulib_namespace_spec_t s={MULIB_SB_NS_USER|checks[i],0,0};uint32_t a=0;
            if(mulib_namespace_enter(&s,&a)){
                if(errno==EPERM||errno==EACCES||errno==EINVAL||errno==ENOSYS)_exit(77);
                _exit(2);
            }
            if(a!=s.flags||getpid()!=original)_exit(3);
            if(checks[i]==MULIB_SB_NS_PID){
                pid_t init=fork();int st;
                if(!init)_exit(getpid()==1?0:1);
                if(init<0||waitpid(init,&st,0)!=init||!WIFEXITED(st)||WEXITSTATUS(st))_exit(4);
            }else{
                FILE *f=fopen("/proc/self/mountinfo","r");if(!f)_exit(5);
                char *line=NULL;size_t cap=0;int mounts=0;
                while(getline(&line,&cap,f)>0){
                    if(strstr(line," shared:")||strstr(line," master:"))_exit(6);
                    mounts++;
                }
                if(!mounts||ferror(f))_exit(7);
                free(line);fclose(f);
            }
            _exit(0);
        }
        int st;CHECK(waitpid(child,&st,0)==child&&WIFEXITED(st));
        if(WEXITSTATUS(st)==77)printf("SKIP namespace positive: host disallows %s setup\n",i?"PID":"private mount");
        else{CHECK(!WEXITSTATUS(st));puts(i?"namespace: only future child entered PID namespace":"namespace: private mount propagation verified via mountinfo");}
    }
}
static void cgroup_test(void){mulib_cgroup2_t g=MULIB_CGROUP2_INIT;mulib_cgroup2_limits_t l={64*MULIB_SANDBOX_MIB,10000,100000,1,1};int fd=open("/tmp",O_RDONLY|O_DIRECTORY);CHECK(fd>=0);CHECK(mulib_cgroup2_create(&g,fd,"bad",&l)==MULIB_SB_INVALID);close(fd);const char*path=getenv("MULIB_TEST_CGROUP_PARENT");if(!path){puts("SKIP cgroup positive: set MULIB_TEST_CGROUP_PARENT to delegated writable cgroup2");return;}fd=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);CHECK(fd>=0);char name[80];snprintf(name,sizeof name,"mulib-test-%ld",(long)getpid());CHECK(!mulib_cgroup2_create(&g,fd,name,&l));CHECK(!mulib_cgroup2_verify(&g,&l));int n;CHECK(!mulib_cgroup2_populated(&g,&n)&&!n);pid_t p=fork();CHECK(p>=0);if(!p)for(;;)pause();CHECK(!mulib_cgroup2_attach(&g,p));CHECK(!mulib_cgroup2_populated(&g,&n)&&n);CHECK(mulib_cgroup2_destroy(&g)==MULIB_SB_STATE);CHECK(!mulib_cgroup2_kill(&g));int st;CHECK(waitpid(p,&st,0)==p&&WIFSIGNALED(st)&&WTERMSIG(st)==SIGKILL);CHECK(!mulib_cgroup2_destroy(&g));close(fd);puts("cgroup: create, limits, attach, population, group kill and cleanup: ok");}
static mulib_sandbox_exit_t await_exit(mulib_sandbox_process_t*p){mulib_sandbox_exit_t s={0};int64_t e=end(3000);for(;;){int r=mulib_sandbox_process_poll(p,&s);if(!r)return s;CHECK(r==MULIB_SB_AGAIN&&mulib_ipc_now_ms()<e);struct timespec t={0,1000000};nanosleep(&t,NULL);}}
static void rollback_test(const mulib_sandbox_launch_t *cfg){
    mulib_sandbox_process_t p=MULIB_SANDBOX_PROCESS_INIT;
    txn t={0};mulib_capbroker_t b;const char *grants[]={"proposal.write"};
    const mulib_capbroker_op_t ops[]={{"submit","proposal.write",op,&t}};
    const mulib_capbroker_hooks_t hooks={begin,commit,abort_tx,audit,&t};
    CHECK(!mulib_capbroker_init(&b,"test-worker",ops,1,grants,1,&hooks,&cfg->profile));
    CHECK(!mulib_capbroker_begin(&b));CHECK(!mulib_sandbox_process_start(&p,cfg));
    CHECK(!mulib_sandbox_process_send(&p,"T",1));
    char req[1],reply[1];size_t n=0,rn=0;
    CHECK(!mulib_sandbox_process_receive(&p,req,sizeof req,&n)&&n==1&&req[0]=='T');
    CHECK(!mulib_capbroker_call(&b,"submit",req,n,reply,sizeof reply,&rn)&&rn==1&&t.stage==1);
    CHECK(!mulib_sandbox_process_send(&p,reply,rn));
    CHECK(mulib_sandbox_process_receive(&p,req,sizeof req,&n)==MULIB_SB_EOF);
    mulib_sandbox_exit_t s=await_exit(&p);CHECK(s.exited&&s.exit_code==9);
    CHECK(mulib_capbroker_finish(&b,s.exited&&!s.exit_code)==MULIB_SB_DENIED);
    CHECK(!t.stage&&!t.committed&&t.aborted==1);
    puts("launcher + broker: worker death rolls back actual staged request");
}
static void launcher_cgroup_test(mulib_sandbox_launch_t cfg){
    const char *path=getenv("MULIB_TEST_CGROUP_PARENT");
    if(!path)return; /* same explicit prerequisite as cgroup_test */
    int fd=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);CHECK(fd>=0);
    char name[80];snprintf(name,sizeof name,"mulib-launch-%ld",(long)getpid());
    cfg.profile.required|=MULIB_SB_ENFORCE_CGROUP;
    mulib_cgroup2_limits_t l={cfg.profile.cgroup_memory_bytes,cfg.profile.cpu_quota_us,cfg.profile.cpu_period_us,1,1};
    mulib_cgroup2_t g=MULIB_CGROUP2_INIT;cfg.group=&g;
    CHECK(!mulib_cgroup2_create(&g,fd,name,&l));close(fd);
    mulib_sandbox_process_t p=MULIB_SANDBOX_PROCESS_INIT;mulib_sandbox_exit_t s={0};int n=-1;
    CHECK(!mulib_sandbox_process_start(&p,&cfg)&&p.enforced==cfg.profile.required);
    CHECK(!mulib_cgroup2_populated(&g,&n)&&n==1);
    CHECK(!mulib_sandbox_process_stop(&p,end(3000),&s)&&!p.pid&&!p.group&&!p.enforced);
    CHECK(s.signal_number==SIGKILL&&!mulib_cgroup2_populated(&g,&n)&&!n);
    CHECK(!mulib_cgroup2_destroy(&g));puts("launcher: actual cgroup admission, cleanup and release");
}
static void launcher_test(const char*worker,const char*fake){int fd=open(worker,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);CHECK(fd>=0);mulib_sandbox_launch_t cfg={0};cfg.trusted_elf_fd=fd;cfg.profile=mulib_sandbox_profile_default();if(geteuid()==0){cfg.drop_identity=1;cfg.uid=65534;cfg.gid=65534;}CHECK(!setenv("MULIB_SECRET","secret",1));int secret=open("/dev/null",O_RDONLY);CHECK(secret>=0);CHECK(dup2(secret,192)==192);close(secret);int cases[]={'A','M','F','N','P','X','S','D','E','L','C'};
    for(size_t i=0;i<sizeof cases/sizeof cases[0];i++){mulib_sandbox_process_t p=MULIB_SANDBOX_PROCESS_INIT;cfg.profile.wall_ms=cases[i]=='L'?120:5000;cfg.profile.cpu_seconds=cases[i]=='C'?1:30;int r=mulib_sandbox_process_start(&p,&cfg);if(r)fprintf(stderr,"start=%s (%d)\n",mulib_sandbox_strerror(r),r);CHECK(!r&&p.enforced==cfg.profile.required);char c=(char)cases[i],reply[32];size_t n=99;CHECK(!mulib_sandbox_process_send(&p,&c,1));if(c=='A'||c=='M'){CHECK(!mulib_sandbox_process_receive(&p,reply,32,&n)&&n==1&&reply[0]==c);mulib_sandbox_exit_t s=await_exit(&p);CHECK(s.exited&&!s.exit_code);}else if(c=='C'){CHECK(mulib_sandbox_process_receive(&p,reply,32,&n)<0);mulib_sandbox_exit_t s=await_exit(&p);CHECK(s.signal_number==SIGKILL||s.signal_number==SIGXCPU);}else if(c=='L'){CHECK(mulib_sandbox_process_receive(&p,reply,32,&n)==MULIB_SB_TIMEOUT);mulib_sandbox_exit_t s={0};CHECK(mulib_sandbox_process_tick(&p,end(3000),&s)==MULIB_SB_TIMEOUT&&!p.pid&&s.signal_number==SIGKILL);}else{CHECK(mulib_sandbox_process_receive(&p,reply,32,&n)<0);mulib_sandbox_exit_t s=await_exit(&p);CHECK(s.signal_number==SIGSYS);}CHECK(p.pid==0&&p.fd==-1);}
    /* A required unavailable Landlock ABI may never silently downgrade. */
    {mulib_sandbox_process_t p=MULIB_SANDBOX_PROCESS_INIT;cfg.profile=mulib_sandbox_profile_default();cfg.profile.required|=MULIB_SB_ENFORCE_LANDLOCK;cfg.profile.landlock_min_abi=65535;CHECK(mulib_sandbox_process_start(&p,&cfg)<0&&!p.pid);cfg.profile=mulib_sandbox_profile_default();}
    /* Standalone PID namespace requires a future child, so launcher must reject. */
    {mulib_sandbox_process_t p=MULIB_SANDBOX_PROCESS_INIT;cfg.profile.required|=MULIB_SB_ENFORCE_NAMESPACE;cfg.profile.namespace_flags=MULIB_SB_NS_PID;cfg.namespace_spec.flags=MULIB_SB_NS_PID;CHECK(mulib_sandbox_process_start(&p,&cfg)==MULIB_SB_INVALID&&!p.pid);cfg.profile=mulib_sandbox_profile_default();cfg.namespace_spec.flags=0;}
    rollback_test(&cfg);launcher_cgroup_test(cfg);
    close(192);close(fd);fd=open(fake,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);CHECK(fd>=0);cfg.trusted_elf_fd=fd;cfg.profile.wall_ms=5000;mulib_sandbox_process_t p=MULIB_SANDBOX_PROCESS_INIT;CHECK(mulib_sandbox_process_start(&p,&cfg)==MULIB_SB_DENIED&&!p.pid&&!p.enforced);close(fd);puts("launcher: echo, memory limit, real SIGSYS denials, deadline/reap, forged readiness rejection: ok");}
int main(int argc,char**argv){CHECK(argc==3);profile_test();ipc_test();interrupt_test();broker_test();seccomp_test();landlock_test();namespace_test();namespace_mount_pid_test();cgroup_test();launcher_test(argv[1],argv[2]);puts("ALL AVAILABLE TESTS PASSED");return 0;}
