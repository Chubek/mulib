/* MuLIB POSIX streaming subprocess transport (pipes or PTY), single-header.
 * Define MULIB_IMPL_PROCESS_STREAM in one TU. Build with _GNU_SOURCE on Linux.
 * No shell parsing; program/argv explicit. argv[0] required, NULL-terminated.
 * The host must approve executable, cwd, env and operating mode beforehand.
 * Program is spawned in a separate process group for group cancellation.
 * Pump returns 1 only after child reaped AND all output drained.
 */
#ifndef MULIB_PROCESS_STREAM_H
#define MULIB_PROCESS_STREAM_H
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum mulib_ps_status {MULIB_PS_DONE=1,MULIB_PS_RUNNING=0,
    MULIB_PS_INVALID=-1,MULIB_PS_SYSTEM=-2,MULIB_PS_LIMIT=-3,
    MULIB_PS_TIMEOUT=-4,MULIB_PS_CANCELLED=-5,MULIB_PS_AGAIN=-6,
    MULIB_PS_CALLBACK=-7,MULIB_PS_UNSUPPORTED=-8} mulib_ps_status_t;
typedef struct mulib_ps_spec {
    const char *file;const char *const *argv;const char *const *env;
    const char *cwd;int stdin_pipe;int pty;unsigned rows,cols;
    uint64_t timeout_ms;size_t max_output_bytes;
} mulib_ps_spec_t;
typedef struct mulib_ps {
    pid_t pid;int input,output,errors;int pty,exited,exit_code,term_signal;
    size_t output_bytes,max_output_bytes;int64_t deadline_ms;
} mulib_ps_t;
#define MULIB_PS_INIT {0,-1,-1,-1,0,0,0,0,0,0,0}
/* Callback receives stream 1=stdout/PTY, 2=stderr; payload borrowed only
 * during callback. Returning 0 requests stop; negative same as 0. */
typedef int (*mulib_ps_chunk_fn)(void *user,int stream,const unsigned char *data,size_t n);
void mulib_ps_init(mulib_ps_t *process);
int mulib_ps_start(mulib_ps_t *process,const mulib_ps_spec_t *spec);
/* Nonblocking write over PTY or socketpair; suppresses SIGPIPE for pipes.
 * bytes_written is set even if only part was written.
 * Returns PS_RUNNING on write progress, PS_AGAIN on EAGAIN, negative on error. */
int mulib_ps_write(mulib_ps_t *process,const void *data,size_t length,size_t *bytes_written);
int mulib_ps_close_input(mulib_ps_t *process);
/* Bounded one-step event pump; waits up to wait_ms. Traps timeout/limit and
 * cancels process group. The caller may pump repeatedly for incremental UI. */
int mulib_ps_pump(mulib_ps_t *process,int wait_ms,mulib_ps_chunk_fn callback,void *user);
int mulib_ps_wait(mulib_ps_t *process,mulib_ps_chunk_fn callback,void *user);
/* Signal child group (SIGTERM by default) then SIGKILL on deadline, reap. */
int mulib_ps_cancel(mulib_ps_t *process,int grace_ms);
/* Always closes handles and makes a best effort to reap child. */
void mulib_ps_destroy(mulib_ps_t *process);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_PROCESS_STREAM) && !defined(MULIB_PROCESS_STREAM_IMPLEMENTED)
#define MULIB_PROCESS_STREAM_IMPLEMENTED
#include "mulib-pty.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <time.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif
static int64_t mulib_ps_now_(void){struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t)<0)return -1;
    if(t.tv_sec<0||t.tv_sec>(INT64_MAX/1000))return -1;
    return (int64_t)t.tv_sec*1000+(int64_t)t.tv_nsec/1000000;}
void mulib_ps_init(mulib_ps_t *p){if(p){memset(p,0,sizeof *p);p->input=p->output=p->errors=-1;}}
static int mulib_ps_pipe_(int fds[2]){
#ifdef __linux__
    return pipe2(fds,O_CLOEXEC)==0;
#else
    if(pipe(fds)<0)return 0;
    if(fcntl(fds[0],F_SETFD,FD_CLOEXEC)<0||fcntl(fds[1],F_SETFD,FD_CLOEXEC)<0){close(fds[0]);close(fds[1]);return 0;}
    return 1;
#endif
}
static int mulib_ps_nonblock_(int fd){if(fd<0)return 1;
    int flags=fcntl(fd,F_GETFL);return flags>=0&&fcntl(fd,F_SETFL,flags|O_NONBLOCK)==0;}
static void mulib_ps_fdclose_(int *fd){if(*fd>=0){close(*fd);*fd=-1;}}
/* Keep internal descriptors out of the stdio redirection destinations, even
 * if the parent entered with one or more standard descriptors closed. */
static int mulib_ps_move_fd_(int *fd){
    if(*fd<0||*fd>STDERR_FILENO)return 1;
    int to=fcntl(*fd,F_DUPFD_CLOEXEC,STDERR_FILENO+1);
    if(to<0)return 0;
    close(*fd);*fd=to;return 1;
}
static int mulib_ps_handshake_(int fd,int64_t deadline){
    int error=0;size_t got=0;
    if(!mulib_ps_nonblock_(fd))return MULIB_PS_SYSTEM;
    for(;;){
        int64_t now=mulib_ps_now_();
        if(now<0)return MULIB_PS_SYSTEM;
        if(now>=deadline)return MULIB_PS_TIMEOUT;
        int64_t remaining=deadline-now;
        struct pollfd ready={fd,POLLIN,0};
        int n=poll(&ready,1,remaining>INT_MAX?INT_MAX:(int)remaining);
        if(n<0&&errno==EINTR)continue;
        if(n<0||(ready.revents&POLLNVAL))return MULIB_PS_SYSTEM;
        if(!n)continue;
        ssize_t count=read(fd,(unsigned char*)&error+got,sizeof error-got);
        if(count<0&&(errno==EINTR||errno==EAGAIN||errno==EWOULDBLOCK))continue;
        if(count<0)return MULIB_PS_SYSTEM;
        if(!count){
            if(!got)return MULIB_PS_RUNNING;
            errno=EIO;return MULIB_PS_SYSTEM;
        }
        got+=(size_t)count;
        if(got==sizeof error){errno=error;return MULIB_PS_SYSTEM;}
    }
}
static const char *mulib_ps_envget_(const char *const *env,const char *key){
    if(!env)return getenv(key);
    size_t n=strlen(key);
    for(size_t i=0;env[i];i++)if(!strncmp(env[i],key,n)&&env[i][n]=='=')return env[i]+n+1;
    return NULL;
}
static void mulib_ps_exec_(const mulib_ps_spec_t *s){
    if(!s->env){execvp(s->file,(char *const*)s->argv);return;}
    if(strchr(s->file,'/')){execve(s->file,(char *const*)s->argv,(char *const*)s->env);return;}
    const char *path=mulib_ps_envget_(s->env,"PATH");if(!path)path="/bin:/usr/bin";
    size_t file_len=strlen(s->file);
    const char *p=path;
    while(1){const char *end=strchr(p,':');size_t n=end?(size_t)(end-p):strlen(p);
        if(n+file_len+2<4096){char full[4096];
            if(n){memcpy(full,p,n);full[n]='/';memcpy(full+n+1,s->file,file_len+1);}
            else {memcpy(full,"./",2);memcpy(full+2,s->file,file_len+1);}
            execve(full,(char *const*)s->argv,(char *const*)s->env);
            if(errno!=ENOENT&&errno!=ENOTDIR&&errno!=EACCES)return;
        }
        if(!end)break;
        p=end+1;
    }
    errno=ENOENT;
}
static int mulib_ps_spec_valid_(const mulib_ps_spec_t *s){
    if(!s||!s->file||!*s->file||!s->argv||!s->argv[0]||(!s->stdin_pipe&&s->pty)||
       (s->pty!=0&&s->pty!=1)||(s->stdin_pipe!=0&&s->stdin_pipe!=1)||
       !s->timeout_ms||!s->max_output_bytes)return 0;
    size_t count=0;
    for(;count<1024 && s->argv[count];count++);
    if(count==1024)return 0;
    if(s->env){size_t i=0;while(i<1024&&s->env[i]){
            if(!strchr(s->env[i],'='))return 0;
            i++;}
        if(i==1024)return 0;}
    if(s->pty&&(!s->rows||!s->cols||s->rows>65535||s->cols>65535))return 0;
    return 1;
}
int mulib_ps_start(mulib_ps_t *p,const mulib_ps_spec_t *s){
    if(!p||p->pid||p->input!=-1||p->output!=-1||p->errors!=-1||!mulib_ps_spec_valid_(s))return MULIB_PS_INVALID;
    int pin[2]={-1,-1},pout[2]={-1,-1},perr[2]={-1,-1},status[2]={-1,-1};
    mulib_pty pair=MULIB_PTY_INIT;int devnull=-1;
    if(!mulib_ps_pipe_(status))goto failed;
    if(s->pty){if(!mulib_pty_open(&pair)||!mulib_pty_resize(pair.slave,s->rows,s->cols))goto failed;}
    else {
        if(s->stdin_pipe&&socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,pin)<0)goto failed;
        if(!mulib_ps_pipe_(pout)||!mulib_ps_pipe_(perr))goto failed;
        if(!s->stdin_pipe&&(devnull=open("/dev/null",O_RDONLY|O_CLOEXEC))<0)goto failed;
    }
    int *fds[]={&status[0],&status[1],&pin[0],&pin[1],&pout[0],&pout[1],
                &perr[0],&perr[1],&pair.master,&pair.slave,&devnull};
    for(size_t i=0;i<sizeof fds/sizeof *fds;i++)if(!mulib_ps_move_fd_(fds[i]))goto failed;
    int64_t now=mulib_ps_now_();
    if(now<0||s->timeout_ms>(uint64_t)(INT64_MAX-now))goto failed;
    pid_t child=fork();if(child<0)goto failed;
    if(!child){
        close(status[0]);
        if(s->pty){
            if(!mulib_pty_child(&pair))goto child_error;
        }else{
            if(setpgid(0,0)<0)goto child_error;
            int in=s->stdin_pipe?pin[0]:devnull;
            if(dup2(in,STDIN_FILENO)<0||dup2(pout[1],STDOUT_FILENO)<0||
               dup2(perr[1],STDERR_FILENO)<0)goto child_error;
        }
        if(pin[0]>=0)close(pin[0]);
        if(pin[1]>=0)close(pin[1]);
        if(pout[0]>=0)close(pout[0]);
        if(pout[1]>=0)close(pout[1]);
        if(perr[0]>=0)close(perr[0]);
        if(perr[1]>=0)close(perr[1]);
        if(devnull>=0)close(devnull);
        mulib_pty_close(&pair);
        if(s->cwd&&chdir(s->cwd)<0)goto child_error;
        mulib_ps_exec_(s);
child_error: {
            int saved=errno;unsigned char *q=(unsigned char*)&saved;size_t n=0;
            while(n<sizeof saved){ssize_t z=write(status[1],q+n,sizeof saved-n);
                if(z<0&&errno==EINTR)continue;
                if(z<=0)break;
                n+=(size_t)z;}
            _exit(127);
        }
    }
    close(status[1]);status[1]=-1;
    if(s->pty){close(pair.slave);pair.slave=-1;p->input=pair.master;p->output=pair.master;pair.master=-1;p->errors=-1;}
    else {
        if(pin[0]>=0){close(pin[0]);pin[0]=-1;}
        if(pout[1]>=0){close(pout[1]);pout[1]=-1;}
        if(perr[1]>=0){close(perr[1]);perr[1]=-1;}
        p->input=s->stdin_pipe?pin[1]:-1;pin[1]=-1;
        p->output=pout[0];pout[0]=-1;
        p->errors=perr[0];perr[0]=-1;
    }
    if(devnull>=0){close(devnull);devnull=-1;}
    p->pid=child;p->pty=s->pty;p->deadline_ms=now+(int64_t)s->timeout_ms;
    p->max_output_bytes=s->max_output_bytes;
    int rc=mulib_ps_handshake_(status[0],p->deadline_ms),saved=errno;
    close(status[0]);
    if(rc==MULIB_PS_RUNNING && (!mulib_ps_nonblock_(p->input)||
       !mulib_ps_nonblock_(p->output)||!mulib_ps_nonblock_(p->errors))){
        rc=MULIB_PS_SYSTEM;saved=errno;
    }
    if(rc!=MULIB_PS_RUNNING){
        mulib_ps_destroy(p);errno=saved;return rc;
    }
    mulib_pty_close(&pair);return MULIB_PS_RUNNING;
failed:
    if(pin[0]>=0)close(pin[0]);
        if(pin[1]>=0)close(pin[1]);
    if(pout[0]>=0)close(pout[0]);
        if(pout[1]>=0)close(pout[1]);
    if(perr[0]>=0)close(perr[0]);
        if(perr[1]>=0)close(perr[1]);
    if(status[0]>=0)close(status[0]);
        if(status[1]>=0)close(status[1]);
    if(devnull>=0)close(devnull);
    mulib_pty_close(&pair);return MULIB_PS_SYSTEM;
}
int mulib_ps_write(mulib_ps_t *p,const void *data,size_t len,size_t *written){
    if(written)*written=0;
    if(!p||p->pid<=0||p->input<0||(!data&&len))return MULIB_PS_INVALID;
    if(!len)return MULIB_PS_RUNNING;
    if(len>(size_t)SSIZE_MAX)len=(size_t)SSIZE_MAX;
    ssize_t n=p->pty?write(p->input,data,len):send(p->input,data,len,MSG_NOSIGNAL);
    if(n<0&&errno==EINTR)return MULIB_PS_AGAIN;
    if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK))return MULIB_PS_AGAIN;
    if(n<0)return MULIB_PS_SYSTEM;
    if(written)*written=(size_t)n;
    return MULIB_PS_RUNNING;
}
int mulib_ps_close_input(mulib_ps_t *p){
    if(!p)return MULIB_PS_INVALID;
    if(p->pty)return MULIB_PS_UNSUPPORTED;/* closing PTY master also closes output */
    mulib_ps_fdclose_(&p->input);return 0;
}
static int mulib_ps_reap_(mulib_ps_t *p){
    if(p->exited||p->pid<=0)return 1;
    int st=0;pid_t r;
    do{r=waitpid(p->pid,&st,WNOHANG);}while(r<0&&errno==EINTR);
    if(r==0)return 0;
    if(r<0){if(errno!=ECHILD)return -1;p->exited=1;return 1;}
    p->exited=1;p->exit_code=WIFEXITED(st)?WEXITSTATUS(st):-1;
    p->term_signal=WIFSIGNALED(st)?WTERMSIG(st):0;
    return 1;
}
int mulib_ps_cancel(mulib_ps_t *p,int grace_ms){
    if(!p||p->pid<=0||grace_ms<0)return MULIB_PS_INVALID;
    if(!p->exited){if(kill(-p->pid,SIGTERM)<0&&errno==ESRCH)kill(p->pid,SIGTERM);
        int64_t end=mulib_ps_now_()+grace_ms;
        while(!mulib_ps_reap_(p)&&mulib_ps_now_()<end){struct timespec ts={0,5000000};nanosleep(&ts,NULL);}
        if(!p->exited){if(kill(-p->pid,SIGKILL)<0&&errno==ESRCH)kill(p->pid,SIGKILL);
            int status;pid_t r;
            do{r=waitpid(p->pid,&status,0);}while(r<0&&errno==EINTR);
            if(r==p->pid){p->exit_code=WIFEXITED(status)?WEXITSTATUS(status):-1;
                p->term_signal=WIFSIGNALED(status)?WTERMSIG(status):0;}
            p->exited=1;
        }
    }
    return 0;
}
static int mulib_ps_drain_(mulib_ps_t *p,int fd,int channel,mulib_ps_chunk_fn cb,void *u){
    unsigned char buf[8192];
    for(int i=0;i<16;i++){
        ssize_t n=read(fd,buf,sizeof buf);
        if(n<0&&errno==EINTR)continue;
        if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK))return 0;
        if(n<0&&p->pty&&errno==EIO)return 1; /* PTY slave closed */
        if(n<0)return MULIB_PS_SYSTEM;
        if(!n)return 1;
        if((size_t)n>p->max_output_bytes-p->output_bytes)return MULIB_PS_LIMIT;
        p->output_bytes+=(size_t)n;
        if(cb&&cb(u,channel,buf,(size_t)n)!=1)return MULIB_PS_CALLBACK;
    }return 0;
}
int mulib_ps_pump(mulib_ps_t *p,int wait_ms,mulib_ps_chunk_fn cb,void *u){
    if(!p||p->pid<=0||wait_ms<0)return MULIB_PS_INVALID;
    int64_t now=mulib_ps_now_();if(now<0)return MULIB_PS_SYSTEM;
    if(now>=p->deadline_ms && (!p->exited||p->output>=0||p->errors>=0)){
        if(!p->exited)mulib_ps_cancel(p,100);
        return MULIB_PS_TIMEOUT;
    }
    struct pollfd fds[2];nfds_t count=0;
    if(p->output>=0){fds[count].fd=p->output;fds[count].events=POLLIN|POLLHUP;count++;}
    if(p->errors>=0){fds[count].fd=p->errors;fds[count].events=POLLIN|POLLHUP;count++;}
    if(count||!p->exited){int64_t left=p->deadline_ms-now;if(left<wait_ms)wait_ms=(int)left;}
    if(!count&&wait_ms>20)wait_ms=20;
    int polled;
    do{polled=poll(fds,count,wait_ms);}while(polled<0&&errno==EINTR);
    if(polled<0)return MULIB_PS_SYSTEM;
    for(nfds_t i=0;i<count;i++){
        if(!fds[i].revents)continue;
        int done=mulib_ps_drain_(p,fds[i].fd,fds[i].fd==p->output?1:2,cb,u);
        if(done<0){mulib_ps_cancel(p,100);return done;}
        if(done==1){if(fds[i].fd==p->output){if(p->input==p->output)p->input=-1;mulib_ps_fdclose_(&p->output);}
            else mulib_ps_fdclose_(&p->errors);}
    }
    int reaped=mulib_ps_reap_(p);if(reaped<0)return MULIB_PS_SYSTEM;
    if(p->exited&&p->output==-1&&p->errors==-1)return MULIB_PS_DONE;
    return MULIB_PS_RUNNING;
}
int mulib_ps_wait(mulib_ps_t *p,mulib_ps_chunk_fn cb,void *u){
    for(;;){int r=mulib_ps_pump(p,50,cb,u);if(r!=MULIB_PS_RUNNING)return r;}
}
void mulib_ps_destroy(mulib_ps_t *p){
    if(!p)return;
    if(p->pid>0&&!p->exited)mulib_ps_cancel(p,100);
    if(p->input>=0&&p->input!=p->output&&p->input!=p->errors)close(p->input);
    if(p->output>=0)close(p->output);
    if(p->errors>=0&&p->errors!=p->output)close(p->errors);
    mulib_ps_init(p);
}
#ifdef __cplusplus
}
#endif
#endif
