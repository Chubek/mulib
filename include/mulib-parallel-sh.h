/* mulib-parallel-sh.h -- bounded POSIX GNU Parallel-style task runner.
 * Public API is C11, implementation is POSIX (fork/exec/waitpid) + pthreads.
 * Build with -D_POSIX_C_SOURCE=200809L -pthread before any system includes.
 * Define MULIB_IMPL_PARALLEL_SH in ONE translation unit. The optional Mune
 * adapter uses mulib-mune.h; define MULIB_PARALLEL_SH_NO_MUNE to omit it.
 * Neither GNU Parallel's full CLI nor its replacement-language semantics are
 * claimed. See the interface's supported options and constraints below.
 */
#ifndef MULIB_PARALLEL_SH_H
#define MULIB_PARALLEL_SH_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#if !defined(MULIB_PARALLEL_SH_NO_MUNE)
#include "mulib-mune.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_parallel_sh_job {
  char *command, *input;
  long pid;
  FILE *out, *err;
  unsigned attempts;
  int finished, exit_code, term_signal, kind; /* 0 shell; 1 Mune Lua */
} mulib_parallel_sh_job_t;
typedef struct mulib_parallel_sh_channel mulib_parallel_sh_channel_t;
typedef struct mulib_parallel_sh {
  mulib_parallel_sh_job_t *jobs;
  size_t count, capacity, max_jobs, completed, failed, active;
  int keep_order, dry_run, halt_on_error, retries, ran, last_status;
  const char *shell;
  FILE *stdout_file, *stderr_file, *joblog;
  mulib_parallel_sh_channel_t **channels;
  size_t channel_count, channel_capacity;
} mulib_parallel_sh_t;
enum {
  MULIB_PARALLEL_SH_OK = 0, MULIB_PARALLEL_SH_INVALID = -1,
  MULIB_PARALLEL_SH_NOMEM = -2, MULIB_PARALLEL_SH_SYSTEM = -3,
  MULIB_PARALLEL_SH_UNSUPPORTED = -4, MULIB_PARALLEL_SH_CLOSED = -5,
  MULIB_PARALLEL_SH_EMPTY = -6, MULIB_PARALLEL_SH_FAILED = -7,
  MULIB_PARALLEL_SH_FULL = -8
};
/** Initialize fresh storage (max_jobs=0 uses online CPUs). Free before reinit. */
int mulib_parallel_sh_init(mulib_parallel_sh_t *p, size_t max_jobs);
/** Copy a shell command. Empty commands are permitted. */
int mulib_parallel_sh_add(mulib_parallel_sh_t *p, const char *command);
/** Execute Lua source as a separate isolated Mune state inside each child. */
int mulib_parallel_sh_add_lua(mulib_parallel_sh_t *p, const char *source);
/** Expand {}, {.}, {/}, {/.}, {//}, {#} with shell-quoted input.
 * Place substitutions outside shell quotes. If there is no substitution,
 * append the shell-quoted input. The returned string is malloc-owned. */
char *mulib_parallel_sh_expand(const char *pattern, const char *input, size_t index);
int mulib_parallel_sh_add_input(mulib_parallel_sh_t *p, const char *pattern,
                                const char *input);
/** Run once with at most max_jobs children. Group each child's stdout
 * and stderr in seekable temporary files (may use disk). Exit 0 when all jobs
 * succeed; FAILED if any job exits nonzero; SYSTEM on OS/output failure.
 * Retains the result in last_status. Call from a single-threaded process. */
int mulib_parallel_sh_run(mulib_parallel_sh_t *p);
void mulib_parallel_sh_free(mulib_parallel_sh_t *p);
/** A named, mutex/condition-variable protected channel of owned NUL strings.
 * Send and receive are nonblocking; receive transfers a malloc'd NUL string.
 * Failure clears receive outputs. Channels are in-process, not child IPC;
 * they are only freed when the scheduler is freed after users stop. */
int mulib_parallel_sh_channel_create(mulib_parallel_sh_t *p, const char *name,
                                     mulib_parallel_sh_channel_t **out);
mulib_parallel_sh_channel_t *mulib_parallel_sh_channel_find(mulib_parallel_sh_t *p,
                                                            const char *name);
int mulib_parallel_sh_channel_send(mulib_parallel_sh_channel_t *c, const char *text);
/** Bound queued messages; zero is unlimited. Refuses limits below current size.
 * Sends to a full channel return FULL without changing the queue. */
int mulib_parallel_sh_channel_set_limit(mulib_parallel_sh_channel_t *c, size_t limit);
int mulib_parallel_sh_channel_receive(mulib_parallel_sh_channel_t *c, char **out);
/** Wait for an item or closure. 0 ms polls; UINT64_MAX waits indefinitely. */
int mulib_parallel_sh_channel_receive_wait(mulib_parallel_sh_channel_t *c,
                                           char **out, uint64_t timeout_ms);
int mulib_parallel_sh_channel_close(mulib_parallel_sh_channel_t *c);
/** Program the scheduler with Mune Lua functions task(command[,input]), lua_task(source),
 * channel(name), send(name,text), recv(name), run(). Lua scripts execute on the
 * caller's thread, not in worker processes. No Mune state is shared with jobs. */
int mulib_parallel_sh_lua(mulib_parallel_sh_t *p, const char *source,
                           size_t length, char *error, size_t error_capacity);
int mulib_parallel_sh_lua_file(mulib_parallel_sh_t *p, const char *path,
                                char *error, size_t error_capacity);
/** Command-line entry point. Supports -j/--jobs, -k, --dry-run, --retries,
 * --halt soon,fail=1, --joblog, -a, -0, :::, ::::, --lua and --lua-code. Unknown options
 * are errors, never silently ignored. No SSH/distributed mode or GNU joblogs. */
int mulib_parallel_sh_main(int argc, char **argv);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_PARALLEL_SH) && !defined(MULIB_PARALLEL_SH_IMPLEMENTED)
#define MULIB_PARALLEL_SH_IMPLEMENTED
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#if defined(__unix__) || defined(__APPLE__)
#define MULIB_PARALLEL_SH_POSIX 1
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <fcntl.h>
#include <time.h>
#else
#define MULIB_PARALLEL_SH_POSIX 0
#endif
struct mulib_parallel_sh_channel {
  char *name;
  char **items;
  size_t start, len, cap, limit;
  int closed;
#if MULIB_PARALLEL_SH_POSIX
  pthread_mutex_t mu;
  pthread_cond_t changed;
#endif
};
#ifndef MULIB_PARALLEL_SH_NO_MUNE
static char *mulib_parallel_sh_copy_bytes_(const char *s,size_t n){
  char *p;
  if(!s || n==SIZE_MAX || memchr(s,0,n))return NULL;
  p=(char*)malloc(n+1);if(!p)return NULL;
  memcpy(p,s,n);p[n]=0;return p;
}
#endif
static char *mulib_parallel_sh_dup_(const char *s) {
  size_t n; char *p;
  if (!s) return NULL;
  n = strlen(s); if (n == SIZE_MAX) return NULL;
  p = (char *)malloc(n+1); if (p) memcpy(p,s,n+1); return p;
}
static int mulib_parallel_sh_reserve_(char **s,size_t *cap,size_t n){
  size_t next=*cap;char *p;
  if(n<=next)return 1;
  if(!next)next=64;
  while(next<n){if(next>SIZE_MAX/2)return 0;next*=2;}
  p=(char*)realloc(*s,next);if(!p)return 0;*s=p;*cap=next;return 1;
}
static int mulib_parallel_sh_put_(char **s,size_t *len,size_t *cap,const char *v,size_t n){
  if(n>SIZE_MAX-*len-1||!mulib_parallel_sh_reserve_(s,cap,*len+n+1))return 0;
  if(n)memcpy(*s+*len,v,n);
  *len+=n;(*s)[*len]=0;return 1;
}
static int mulib_parallel_sh_quote_(char **s,size_t *len,size_t *cap,const char *v){
  size_t i;
  if(!mulib_parallel_sh_put_(s,len,cap,"'",1))return 0;
  for(i=0;v[i];i++){
    if(v[i]=='\''){if(!mulib_parallel_sh_put_(s,len,cap,"'\\''",4))return 0;}
    else if(!mulib_parallel_sh_put_(s,len,cap,v+i,1))return 0;
  }
  return mulib_parallel_sh_put_(s,len,cap,"'",1);
}
char *mulib_parallel_sh_expand(const char *pat,const char *input,size_t number){
  char *result=NULL;size_t len=0,cap=0,i=0;int found=0;char numeral[48];
  if(!pat||!input)return NULL;
  snprintf(numeral,sizeof numeral,"%zu",number);
  while(pat[i]){
    const char *v=NULL;size_t span=0;int quote=1;char *temp=NULL;
    if(pat[i]=='{' && pat[i+1]=='#' && pat[i+2]=='}'){v=numeral;span=3;quote=0;}
    else if(pat[i]=='{' && pat[i+1]=='/' && pat[i+2]=='/' && pat[i+3]=='}'){
      const char *slash=strrchr(input,'/');v=slash?(slash==input?"/":input):".";
      if(slash&&slash!=input){temp=(char*)malloc((size_t)(slash-input)+1);if(!temp)goto failed;memcpy(temp,input,(size_t)(slash-input));temp[slash-input]=0;v=temp;}
      span=4;
    }else if(pat[i]=='{' && pat[i+1]=='/' && pat[i+2]=='}'){
      const char *slash=strrchr(input,'/');v=slash?slash+1:input;span=3;
    }else if(!strncmp(pat+i,"{/.}",4)){
      const char *slash=strrchr(input,'/');const char *base=slash?slash+1:input;
      const char *dot=strrchr(base,'.');size_t length=dot&&dot!=base?(size_t)(dot-base):strlen(base);
      temp=(char*)malloc(length+1);if(!temp)goto failed;
      memcpy(temp,base,length);temp[length]=0;v=temp;span=4;
    }else if(pat[i]=='{' && pat[i+1]=='.' && pat[i+2]=='}'){
      const char *slash=strrchr(input,'/');const char *base=slash?slash+1:input;
      const char *dot=strrchr(base,'.');size_t length=dot&&dot!=base?(size_t)(dot-input):strlen(input);
      temp=(char*)malloc(length+1);if(!temp)goto failed;
      memcpy(temp,input,length);temp[length]=0;v=temp;span=3;
    }else if(pat[i]=='{' && pat[i+1]=='}'){v=input;span=2;}
    if(v){int ok=quote?mulib_parallel_sh_quote_(&result,&len,&cap,v):mulib_parallel_sh_put_(&result,&len,&cap,v,strlen(v));
      free(temp);if(!ok)goto failed;found=1;i+=span;
    }else if(!mulib_parallel_sh_put_(&result,&len,&cap,pat+i++,1))goto failed;
  }
  if(!found){if(len&&!mulib_parallel_sh_put_(&result,&len,&cap," ",1))goto failed;
    if(!mulib_parallel_sh_quote_(&result,&len,&cap,input))goto failed;}
  if(!result){result=mulib_parallel_sh_dup_("");}
  return result;
failed:free(result);return NULL;
}
int mulib_parallel_sh_init(mulib_parallel_sh_t *p,size_t max_jobs){
  if(!p)return MULIB_PARALLEL_SH_INVALID;
  memset(p,0,sizeof *p);
#if MULIB_PARALLEL_SH_POSIX
  if(!max_jobs){long n=sysconf(_SC_NPROCESSORS_ONLN);max_jobs=n>0?(size_t)n:1;}
#else
  if(!max_jobs)max_jobs=1;
#endif
  p->max_jobs=max_jobs;p->shell="/bin/sh";p->stdout_file=stdout;p->stderr_file=stderr;
  return MULIB_PARALLEL_SH_OK;
}
int mulib_parallel_sh_add(mulib_parallel_sh_t *p,const char *command){
  mulib_parallel_sh_job_t *a;char *s;size_t cap;
  if(!p||!p->max_jobs||p->ran||!command||p->count>=SIZE_MAX/sizeof *a)return MULIB_PARALLEL_SH_INVALID;
  s=mulib_parallel_sh_dup_(command);if(!s)return MULIB_PARALLEL_SH_NOMEM;
  if(p->count==p->capacity){cap=p->capacity?p->capacity*2:16;
    if(cap<p->capacity||cap>SIZE_MAX/sizeof *a){free(s);return MULIB_PARALLEL_SH_NOMEM;}
    a=(mulib_parallel_sh_job_t*)realloc(p->jobs,cap*sizeof *a);
    if(!a){free(s);return MULIB_PARALLEL_SH_NOMEM;}
    p->capacity=cap;p->jobs=a;
  }
  memset(&p->jobs[p->count],0,sizeof *p->jobs);p->jobs[p->count++].command=s;
  return MULIB_PARALLEL_SH_OK;
}
int mulib_parallel_sh_add_lua(mulib_parallel_sh_t *p,const char *source){
#ifdef MULIB_PARALLEL_SH_NO_MUNE
  (void)p;(void)source;return MULIB_PARALLEL_SH_UNSUPPORTED;
#else
  int rc=mulib_parallel_sh_add(p,source);
  if(!rc)p->jobs[p->count-1].kind=1;
  return rc;
#endif
}
int mulib_parallel_sh_add_input(mulib_parallel_sh_t *p,const char *pattern,const char *input){
  char *s;int rc;
  if(!p||!pattern||!input)return MULIB_PARALLEL_SH_INVALID;
  s=mulib_parallel_sh_expand(pattern,input,p->count+1);
  if(!s)return MULIB_PARALLEL_SH_NOMEM;
  rc=mulib_parallel_sh_add(p,s);free(s);
  if(rc==MULIB_PARALLEL_SH_OK){p->jobs[p->count-1].input=mulib_parallel_sh_dup_(input);
    if(!p->jobs[p->count-1].input){free(p->jobs[p->count-1].command);p->count--;return MULIB_PARALLEL_SH_NOMEM;}}
  return rc;
}
#if MULIB_PARALLEL_SH_POSIX
/* Keep capture descriptors away from standard descriptors, even if the host
 * entered with stdin/stdout/stderr closed. */
static FILE *mulib_parallel_sh_capture_(void){
  FILE *f=tmpfile();int fd;
  if(!f)return NULL;
  fd=fcntl(fileno(f),F_DUPFD_CLOEXEC,3);
  fclose(f);
  if(fd<0)return NULL;
  f=fdopen(fd,"w+b");if(!f)close(fd);
  return f;
}
static int mulib_parallel_sh_spawn_(mulib_parallel_sh_t *p,mulib_parallel_sh_job_t *j){
  pid_t pid;const char *sh=p->shell?p->shell:"/bin/sh";
  j->out=mulib_parallel_sh_capture_();j->err=mulib_parallel_sh_capture_();
  if(!j->out||!j->err){if(j->out)fclose(j->out);
  if(j->err)fclose(j->err);
  j->out=j->err=NULL;return 0;}
  fflush(NULL);
  pid=fork();
  if(pid<0){fclose(j->out);fclose(j->err);j->out=j->err=NULL;return 0;}
  if(pid==0){
    if(dup2(fileno(j->out),STDOUT_FILENO)<0||dup2(fileno(j->err),STDERR_FILENO)<0)_exit(126);
    close(fileno(j->out));close(fileno(j->err));
    {int devnull=open("/dev/null",O_RDONLY);
      if(devnull<0||dup2(devnull,STDIN_FILENO)<0)_exit(126);
      if(devnull!=STDIN_FILENO)(void)close(devnull);
    }
#ifndef MULIB_PARALLEL_SH_NO_MUNE
    if(j->kind==1){
      mulib_mune_state_t *state=mulib_mune_state_create(NULL);
      mulib_mune_result_t *result=NULL;mulib_mune_error_t err={0};int okay=0;
      if(state){okay=mulib_mune_run_source(state,j->command,strlen(j->command),&result,&err)==MULIB_MUNE_OK;
        if(!okay)fprintf(stderr,"parallel-sh Lua task: %s\n",err.message?err.message:"runtime failure");
        mulib_mune_result_destroy(result);mulib_mune_state_destroy(state);
      }else fputs("parallel-sh Lua task: state allocation failed\n",stderr);
      fflush(stdout);fflush(stderr);_exit(okay?0:1);
    }
#endif
    execlp(sh,sh,"-c",j->command,(char*)NULL);_exit(127);
  }
  j->pid=(long)pid;j->attempts++;return 1;
}
static int mulib_parallel_sh_copy_(FILE *f,FILE *target){
  unsigned char buf[16384];size_t n;
  if(!f||!target)return 0;
  if(fseek(f,0,SEEK_SET))return MULIB_PARALLEL_SH_SYSTEM;
  while((n=fread(buf,1,sizeof buf,f))>0)
    if(fwrite(buf,1,n,target)!=n)return MULIB_PARALLEL_SH_SYSTEM;
  return ferror(f)?MULIB_PARALLEL_SH_SYSTEM:0;
}
static int mulib_parallel_sh_flush_(mulib_parallel_sh_t *p,mulib_parallel_sh_job_t *j){
  int rc=mulib_parallel_sh_copy_(j->out,p->stdout_file);
  if(mulib_parallel_sh_copy_(j->err,p->stderr_file))rc=MULIB_PARALLEL_SH_SYSTEM;
  if(j->out&&fclose(j->out))rc=MULIB_PARALLEL_SH_SYSTEM;
  if(j->err&&fclose(j->err))rc=MULIB_PARALLEL_SH_SYSTEM;
  j->out=j->err=NULL;
  if(p->joblog){
    fprintf(p->joblog,"%zu\t%d\t%u\t",(size_t)(j-p->jobs)+1,j->exit_code,j->attempts);
    for(const char *s=j->command;*s;s++){
      if(*s=='\n')fputs("\\n",p->joblog);
      else if(*s=='\r')fputs("\\r",p->joblog);
      else if(*s=='\t')fputs("\\t",p->joblog);
      else if(*s=='\\')fputs("\\\\",p->joblog);
      else fputc((unsigned char)*s,p->joblog);
    }
    if(fputc('\n',p->joblog)==EOF||ferror(p->joblog))rc=MULIB_PARALLEL_SH_SYSTEM;
  }
  return rc;
}
#endif
int mulib_parallel_sh_run(mulib_parallel_sh_t *p){
  if(!p||p->ran||!p->max_jobs||p->retries<0)return MULIB_PARALLEL_SH_INVALID;
  p->ran=1;
  if(p->dry_run){
    if(p->stdout_file){
      for(size_t i=0;i<p->count;i++)
        if(fprintf(p->stdout_file,"%s\n",p->jobs[i].command)<0)p->last_status=MULIB_PARALLEL_SH_SYSTEM;
      if(fflush(p->stdout_file))p->last_status=MULIB_PARALLEL_SH_SYSTEM;
    }
    return p->last_status;
  }
#if !MULIB_PARALLEL_SH_POSIX
  return p->last_status=MULIB_PARALLEL_SH_UNSUPPORTED;
#else
  size_t next=0,printed=0,running=0;int failure=0,system_error=0;
  while(next<p->count||running){
    while(next<p->count&&running<p->max_jobs&&!system_error&&!(failure&&p->halt_on_error)){
      if(!mulib_parallel_sh_spawn_(p,&p->jobs[next])){system_error=1;break;}
      running++;p->active=running;next++;
    }
    if(!running)break;
    {
      int raw;pid_t got;size_t i;
      /* Reap only our children; never steal another library's SIGCHLD. */
      got=0;
      for(i=0;i<next;i++)if(p->jobs[i].pid>0){
        pid_t rc=waitpid((pid_t)p->jobs[i].pid,&raw,WNOHANG);
        if(rc>0){got=rc;break;}
        if(rc<0&&errno!=EINTR){
          got=-1;break;
        }
      }
      if(got<0){system_error=1;break;}
      if(got==0){struct timeval tv={0,10000};(void)select(0,NULL,NULL,NULL,&tv);continue;}
      {mulib_parallel_sh_job_t *j=&p->jobs[i];
        j->pid=0;running--;p->active=running;
        j->term_signal=WIFSIGNALED(raw)?WTERMSIG(raw):0;
        j->exit_code=WIFEXITED(raw)?WEXITSTATUS(raw):128+j->term_signal;
        if(j->exit_code&&j->attempts<=(unsigned)p->retries){
          fclose(j->out);fclose(j->err);j->out=j->err=NULL;
          if(mulib_parallel_sh_spawn_(p,j)){running++;p->active=running;continue;}
          j->exit_code=127;system_error=1;
        }
        j->finished=1;p->completed++;if(j->exit_code){p->failed++;failure=1;}
        if(!p->keep_order){if(mulib_parallel_sh_flush_(p,j))system_error=1;}
        else while(printed<next&&p->jobs[printed].finished){
          if(mulib_parallel_sh_flush_(p,&p->jobs[printed]))system_error=1;
          printed++;
        }
      }
    }
  }
  /* If waitpid fails, kill/reap only jobs owned by this scheduler. */
  for(size_t i=0;i<next;i++)if(p->jobs[i].pid>0){
    int raw;pid_t owned=(pid_t)p->jobs[i].pid;pid_t w;
    /* Confirm child is still our waitable child before sending a signal. */
    do {w=waitpid(owned,&raw,WNOHANG);}while(w<0&&errno==EINTR);
    if(!w){
      (void)kill(owned,SIGKILL);
      do {w=waitpid(owned,&raw,0);}while(w<0&&errno==EINTR);
    }
    p->jobs[i].pid=0;p->jobs[i].exit_code=127;p->jobs[i].finished=1;
    p->completed++;p->failed++;
    if(!p->keep_order&&mulib_parallel_sh_flush_(p,&p->jobs[i]))system_error=1;
  }
  if(p->keep_order)while(printed<next&&p->jobs[printed].finished){
    if(mulib_parallel_sh_flush_(p,&p->jobs[printed]))system_error=1;
    printed++;
  }
  p->active=0;
  if(p->stdout_file&&fflush(p->stdout_file))system_error=1;
  if(p->stderr_file&&fflush(p->stderr_file))system_error=1;
  if(p->joblog&&fflush(p->joblog))system_error=1;
  return p->last_status=system_error?MULIB_PARALLEL_SH_SYSTEM:
    failure?MULIB_PARALLEL_SH_FAILED:MULIB_PARALLEL_SH_OK;
#endif
}
mulib_parallel_sh_channel_t *mulib_parallel_sh_channel_find(mulib_parallel_sh_t *p,const char *name){
  if(!p||!name)return NULL;
  for(size_t i=0;i<p->channel_count;i++)if(!strcmp(p->channels[i]->name,name))return p->channels[i];
  return NULL;
}
int mulib_parallel_sh_channel_create(mulib_parallel_sh_t *p,const char *name,mulib_parallel_sh_channel_t **out){
  mulib_parallel_sh_channel_t *c,**a;size_t cap;
  if(out)*out=NULL;
  if(!p||!p->max_jobs||!name||!*name||!out)return MULIB_PARALLEL_SH_INVALID;
  c=mulib_parallel_sh_channel_find(p,name);if(c){*out=c;return 0;}
  c=(mulib_parallel_sh_channel_t*)calloc(1,sizeof *c);if(!c)return MULIB_PARALLEL_SH_NOMEM;
  c->name=mulib_parallel_sh_dup_(name);if(!c->name){free(c);return MULIB_PARALLEL_SH_NOMEM;}
#if MULIB_PARALLEL_SH_POSIX
  if(pthread_mutex_init(&c->mu,NULL)){free(c->name);free(c);return MULIB_PARALLEL_SH_SYSTEM;}
  if(pthread_cond_init(&c->changed,NULL)){
    pthread_mutex_destroy(&c->mu);free(c->name);free(c);return MULIB_PARALLEL_SH_SYSTEM;
  }
#endif
  if(p->channel_count==p->channel_capacity){
    cap=p->channel_capacity?p->channel_capacity*2:8;
    if(cap<p->channel_capacity || cap>SIZE_MAX/sizeof *a){
#if MULIB_PARALLEL_SH_POSIX
      pthread_cond_destroy(&c->changed);pthread_mutex_destroy(&c->mu);
#endif
      free(c->name);free(c);return MULIB_PARALLEL_SH_NOMEM;
    }
    a=(mulib_parallel_sh_channel_t**)realloc(p->channels,cap*sizeof *a);
    if(!a){
#if MULIB_PARALLEL_SH_POSIX
      pthread_cond_destroy(&c->changed);
      pthread_mutex_destroy(&c->mu);
#endif
      free(c->name);free(c);return MULIB_PARALLEL_SH_NOMEM;
    }
    p->channels=a;p->channel_capacity=cap;
  }
  p->channels[p->channel_count++]=c;*out=c;return 0;
}
int mulib_parallel_sh_channel_set_limit(mulib_parallel_sh_channel_t *c,size_t limit){
  int rc=0;
  if(!c)return MULIB_PARALLEL_SH_INVALID;
#if MULIB_PARALLEL_SH_POSIX
  pthread_mutex_lock(&c->mu);
#endif
  if(limit&&limit<c->len-c->start)rc=MULIB_PARALLEL_SH_INVALID;
  else c->limit=limit;
#if MULIB_PARALLEL_SH_POSIX
  pthread_mutex_unlock(&c->mu);
#endif
  return rc;
}
int mulib_parallel_sh_channel_send(mulib_parallel_sh_channel_t *c,const char *text){
  char **a,*dup=NULL;size_t cap;int ret=0;
  if(!c||!text)return MULIB_PARALLEL_SH_INVALID;
#if MULIB_PARALLEL_SH_POSIX
  pthread_mutex_lock(&c->mu);
#endif
  if(c->closed)ret=MULIB_PARALLEL_SH_CLOSED;
  else if(c->limit&&c->len-c->start>=c->limit)ret=MULIB_PARALLEL_SH_FULL;
  else {
    dup=mulib_parallel_sh_dup_(text);if(!dup)ret=MULIB_PARALLEL_SH_NOMEM;
    if(!ret&&c->start&&c->len==c->cap){
      memmove(c->items,c->items+c->start,(c->len-c->start)*sizeof *a);
      c->len-=c->start;c->start=0;
    }
    if(!ret&&c->len==c->cap){cap=c->cap?c->cap*2:8;
      if(cap<c->cap||cap>SIZE_MAX/sizeof *a)ret=MULIB_PARALLEL_SH_NOMEM;
      else {a=(char**)realloc(c->items,cap*sizeof *a);if(!a)ret=MULIB_PARALLEL_SH_NOMEM;
        else {c->items=a;c->cap=cap;}}}
    if(!ret){c->items[c->len++]=dup;
#if MULIB_PARALLEL_SH_POSIX
      pthread_cond_signal(&c->changed);
#endif
    }
  }
#if MULIB_PARALLEL_SH_POSIX
  pthread_mutex_unlock(&c->mu);
#endif
  if(ret)free(dup);
  return ret;
}
int mulib_parallel_sh_channel_receive(mulib_parallel_sh_channel_t *c,char **out){
  int rc=0;
  if(out)*out=NULL;
  if(!c||!out)return MULIB_PARALLEL_SH_INVALID;
#if MULIB_PARALLEL_SH_POSIX
  pthread_mutex_lock(&c->mu);
#endif
  if(c->start<c->len){*out=c->items[c->start++];
    if(c->start==c->len)c->start=c->len=0;
  }else rc=c->closed?MULIB_PARALLEL_SH_CLOSED:MULIB_PARALLEL_SH_EMPTY;
#if MULIB_PARALLEL_SH_POSIX
  pthread_mutex_unlock(&c->mu);
#endif
  return rc;
}
int mulib_parallel_sh_channel_receive_wait(mulib_parallel_sh_channel_t *c,char **out,uint64_t ms){
  if(out)*out=NULL;
  if(!c||!out)return MULIB_PARALLEL_SH_INVALID;
#if MULIB_PARALLEL_SH_POSIX
  int rc=0;
  pthread_mutex_lock(&c->mu);
  if(ms==UINT64_MAX){
    while(c->start==c->len&&!c->closed)
      if(pthread_cond_wait(&c->changed,&c->mu)){rc=MULIB_PARALLEL_SH_SYSTEM;break;}
  }else if(ms){
    struct timespec deadline;uint64_t seconds;
    if(timespec_get(&deadline,TIME_UTC)!=TIME_UTC)rc=MULIB_PARALLEL_SH_SYSTEM;
    else {
      deadline.tv_nsec += (long)((ms%1000)*1000000);
      seconds=ms/1000+(deadline.tv_nsec>=1000000000L);
      if(deadline.tv_nsec>=1000000000L)deadline.tv_nsec-=1000000000L;
      if(deadline.tv_sec<0||seconds>UINT64_MAX-(uint64_t)deadline.tv_sec)rc=MULIB_PARALLEL_SH_INVALID;
      else {
        seconds+=(uint64_t)deadline.tv_sec;
        deadline.tv_sec=(time_t)seconds;
        if(deadline.tv_sec<0||(uint64_t)deadline.tv_sec!=seconds)rc=MULIB_PARALLEL_SH_INVALID;
      }
      while(!rc&&c->start==c->len&&!c->closed){
        int w=pthread_cond_timedwait(&c->changed,&c->mu,&deadline);
        if(w==ETIMEDOUT)break;
        if(w){rc=MULIB_PARALLEL_SH_SYSTEM;break;}
      }
    }
  }
  if(!rc){
    if(c->start<c->len){*out=c->items[c->start++];
      if(c->start==c->len)c->start=c->len=0;
    }else rc=c->closed?MULIB_PARALLEL_SH_CLOSED:MULIB_PARALLEL_SH_EMPTY;
  }
  pthread_mutex_unlock(&c->mu);
  return rc;
#else
  (void)ms;
  return mulib_parallel_sh_channel_receive(c,out);
#endif
}
int mulib_parallel_sh_channel_close(mulib_parallel_sh_channel_t *c){
  if(!c)return MULIB_PARALLEL_SH_INVALID;
#if MULIB_PARALLEL_SH_POSIX
  pthread_mutex_lock(&c->mu);
#endif
  c->closed=1;
#if MULIB_PARALLEL_SH_POSIX
  pthread_cond_broadcast(&c->changed);
  pthread_mutex_unlock(&c->mu);
#endif
  return 0;
}
void mulib_parallel_sh_free(mulib_parallel_sh_t *p){
  if(!p)return;
  for(size_t i=0;i<p->count;i++){
    free(p->jobs[i].command);free(p->jobs[i].input);
    if(p->jobs[i].out)fclose(p->jobs[i].out);
    if(p->jobs[i].err)fclose(p->jobs[i].err);
  }
  free(p->jobs);
  for(size_t i=0;i<p->channel_count;i++){
    mulib_parallel_sh_channel_t *c=p->channels[i];
#if MULIB_PARALLEL_SH_POSIX
    pthread_cond_destroy(&c->changed);
    pthread_mutex_destroy(&c->mu);
#endif
    for(size_t j=c->start;j<c->len;j++)free(c->items[j]);
    free(c->items);free(c->name);free(c);
  }
  free(p->channels);memset(p,0,sizeof *p);
}
static int mulib_parallel_sh_read_inputs_(mulib_parallel_sh_t *p,const char *pat,FILE *fp,int delim){
  char *s=NULL;size_t len=0,cap=0;int ch,rc=0;
  while((ch=fgetc(fp))!=EOF){
    if(ch==0&&delim!=0){rc=MULIB_PARALLEL_SH_INVALID;break;}
    if(ch==delim){if(len&&s[len-1]=='\r'&&delim=='\n')s[--len]=0;
      if(len||delim=='\0'){rc=mulib_parallel_sh_add_input(p,pat,s?s:"");if(rc)break;}
      len=0;if(s)s[0]=0;
    }else if(len>=1048576){rc=MULIB_PARALLEL_SH_INVALID;break;}
    else if(!mulib_parallel_sh_put_(&s,&len,&cap,(char*)&(unsigned char){(unsigned char)ch},1)){rc=MULIB_PARALLEL_SH_NOMEM;break;}
  }
  if(!rc&&len)rc=mulib_parallel_sh_add_input(p,pat,s);
  if(!rc&&ferror(fp))rc=MULIB_PARALLEL_SH_SYSTEM;
  free(s);return rc;
}
#ifndef MULIB_PARALLEL_SH_NO_MUNE
static mulib_mune_status_t mulib_parallel_sh_lua_task_(mulib_mune_state_t *st,const mulib_mune_value_t *a,size_t n,mulib_mune_result_t *out,void *ctx){
  mulib_parallel_sh_t *p=(mulib_parallel_sh_t*)ctx;size_t sz;const char *str;char *cmd=NULL,*input=NULL;int rc;
  (void)st;(void)out;
  if(n<1||n>2||a[0].type!=MULIB_MUNE_STRING)return MULIB_MUNE_INVALID;
  str=mulib_mune_value_string(a[0],&sz);cmd=mulib_parallel_sh_copy_bytes_(str,sz);
  if(!cmd)return MULIB_MUNE_INVALID;
  if(n==2){
    if(a[1].type!=MULIB_MUNE_STRING){free(cmd);return MULIB_MUNE_INVALID;}
    str=mulib_mune_value_string(a[1],&sz);input=mulib_parallel_sh_copy_bytes_(str,sz);
    if(!input){free(cmd);return MULIB_MUNE_INVALID;}
    rc=mulib_parallel_sh_add_input(p,cmd,input);
  }else rc=mulib_parallel_sh_add(p,cmd);
  free(cmd);free(input);
  return rc==0?MULIB_MUNE_OK:MULIB_MUNE_RUNTIME;
}
static mulib_mune_status_t mulib_parallel_sh_lua_task_lua_(mulib_mune_state_t *st,
  const mulib_mune_value_t *a,size_t n,mulib_mune_result_t *out,void *ctx){
  size_t sz;const char *v;char *source;int rc;(void)st;(void)out;
  if(n!=1||a[0].type!=MULIB_MUNE_STRING)return MULIB_MUNE_INVALID;
  v=mulib_mune_value_string(a[0],&sz);
  source=mulib_parallel_sh_copy_bytes_(v,sz);
  if(!source)return MULIB_MUNE_INVALID;
  rc=mulib_parallel_sh_add_lua((mulib_parallel_sh_t*)ctx,source);free(source);
  return rc==0?MULIB_MUNE_OK:MULIB_MUNE_RUNTIME;
}
static mulib_mune_status_t mulib_parallel_sh_lua_channel_(mulib_mune_state_t *st,const mulib_mune_value_t *a,size_t n,mulib_mune_result_t *out,void *ctx){
  mulib_parallel_sh_channel_t *c;size_t sz;const char *text;char *name;int rc;
  (void)st;(void)out;
  if(n!=1||a[0].type!=MULIB_MUNE_STRING)return MULIB_MUNE_INVALID;
  text=mulib_mune_value_string(a[0],&sz);name=mulib_parallel_sh_copy_bytes_(text,sz);
  if(!name)return MULIB_MUNE_INVALID;
  rc=mulib_parallel_sh_channel_create((mulib_parallel_sh_t*)ctx,name,&c);free(name);
  return rc==0?MULIB_MUNE_OK:MULIB_MUNE_RUNTIME;
}
static mulib_mune_status_t mulib_parallel_sh_lua_send_(mulib_mune_state_t *st,const mulib_mune_value_t *a,size_t n,mulib_mune_result_t *out,void *ctx){
  mulib_parallel_sh_channel_t *c;const char *text;char *name,*msg;size_t sz;int rc;
  (void)st;(void)out;
  if(n!=2||a[0].type!=MULIB_MUNE_STRING||a[1].type!=MULIB_MUNE_STRING)return MULIB_MUNE_INVALID;
  text=mulib_mune_value_string(a[0],&sz);name=mulib_parallel_sh_copy_bytes_(text,sz);
  if(!name)return MULIB_MUNE_INVALID;
  c=mulib_parallel_sh_channel_find((mulib_parallel_sh_t*)ctx,name);free(name);
  if(!c)return MULIB_MUNE_NAME;
  text=mulib_mune_value_string(a[1],&sz);msg=mulib_parallel_sh_copy_bytes_(text,sz);
  if(!msg)return MULIB_MUNE_INVALID;
  rc=mulib_parallel_sh_channel_send(c,msg);free(msg);
  return rc==0?MULIB_MUNE_OK:MULIB_MUNE_RUNTIME;
}
static mulib_mune_status_t mulib_parallel_sh_lua_recv_(mulib_mune_state_t *st,const mulib_mune_value_t *a,size_t n,mulib_mune_result_t *out,void *ctx){
  mulib_parallel_sh_channel_t *c;const char *text;char *name,*v=NULL;size_t sz;
  mulib_mune_value_t value;int rc;
  if(n!=1||a[0].type!=MULIB_MUNE_STRING)return MULIB_MUNE_INVALID;
  text=mulib_mune_value_string(a[0],&sz);name=mulib_parallel_sh_copy_bytes_(text,sz);
  if(!name)return MULIB_MUNE_INVALID;
  c=mulib_parallel_sh_channel_find((mulib_parallel_sh_t*)ctx,name);free(name);
  if(!c)return MULIB_MUNE_NAME;
  rc=mulib_parallel_sh_channel_receive(c,&v);
  if(rc==MULIB_PARALLEL_SH_EMPTY||rc==MULIB_PARALLEL_SH_CLOSED)return mulib_mune_result_push(out,mulib_mune_nil())?MULIB_MUNE_OK:MULIB_MUNE_MEMORY;
  if(rc)return MULIB_MUNE_RUNTIME;
  if(mulib_mune_make_string(st,v,strlen(v),&value)!=MULIB_MUNE_OK){free(v);return MULIB_MUNE_MEMORY;}
  free(v);return mulib_mune_result_push(out,value)?MULIB_MUNE_OK:MULIB_MUNE_MEMORY;
}
static mulib_mune_status_t mulib_parallel_sh_lua_run_(mulib_mune_state_t *st,const mulib_mune_value_t *a,size_t n,mulib_mune_result_t *out,void *ctx){
  mulib_parallel_sh_t *p=(mulib_parallel_sh_t*)ctx;int rc;
  (void)st;(void)a;if(n)return MULIB_MUNE_INVALID;
  rc=mulib_parallel_sh_run(p);
  return mulib_mune_result_push(out,mulib_mune_boolean(rc==0))?MULIB_MUNE_OK:MULIB_MUNE_MEMORY;
}
int mulib_parallel_sh_lua(mulib_parallel_sh_t *p,const char *source,size_t len,char *error,size_t cap){
  mulib_mune_state_t *st;mulib_mune_result_t *result=NULL;mulib_mune_error_t e={0};int rc;
  if(error&&cap)error[0]=0;
  if(!p||!source)return MULIB_PARALLEL_SH_INVALID;
  st=mulib_mune_state_create(NULL);if(!st)return MULIB_PARALLEL_SH_NOMEM;
  rc=mulib_mune_bind(st,"task",mulib_parallel_sh_lua_task_,p)!=MULIB_MUNE_OK||
    mulib_mune_bind(st,"lua_task",mulib_parallel_sh_lua_task_lua_,p)!=MULIB_MUNE_OK||
    mulib_mune_bind(st,"channel",mulib_parallel_sh_lua_channel_,p)!=MULIB_MUNE_OK||
    mulib_mune_bind(st,"send",mulib_parallel_sh_lua_send_,p)!=MULIB_MUNE_OK||
    mulib_mune_bind(st,"recv",mulib_parallel_sh_lua_recv_,p)!=MULIB_MUNE_OK||
    mulib_mune_bind(st,"run",mulib_parallel_sh_lua_run_,p)!=MULIB_MUNE_OK;
  if(!rc)rc=mulib_mune_run_source(st,source,len,&result,&e)!=MULIB_MUNE_OK;
  if(error&&cap)snprintf(error,cap,"%s",rc?(e.message?e.message:"Lua binding or runtime error"):"");
  mulib_mune_result_destroy(result);mulib_mune_state_destroy(st);
  return rc?MULIB_PARALLEL_SH_FAILED:0;
}
#else
int mulib_parallel_sh_lua(mulib_parallel_sh_t *p,const char *s,size_t n,char *e,size_t cap){
  (void)p;(void)s;(void)n;if(e&&cap)e[0]=0;return MULIB_PARALLEL_SH_UNSUPPORTED;
}
#endif
int mulib_parallel_sh_lua_file(mulib_parallel_sh_t *p,const char *path,char *error,size_t cap){
  FILE *f;long n;char *buf;size_t r;int rc;
  if(error&&cap)snprintf(error,cap,"%s","cannot read Lua source (maximum 1 MiB)");
  if(!p||!path)return MULIB_PARALLEL_SH_INVALID;
  if(!(f=fopen(path,"rb")))return MULIB_PARALLEL_SH_SYSTEM;
  if(fseek(f,0,SEEK_END)||((n=ftell(f))<0)||n>1048576||fseek(f,0,SEEK_SET)){fclose(f);return MULIB_PARALLEL_SH_SYSTEM;}
  buf=(char*)malloc((size_t)n+1);if(!buf){fclose(f);return MULIB_PARALLEL_SH_NOMEM;}
  r=fread(buf,1,(size_t)n,f);fclose(f);buf[r]=0;
  rc=r==(size_t)n?mulib_parallel_sh_lua(p,buf,r,error,cap):MULIB_PARALLEL_SH_SYSTEM;
  free(buf);return rc;
}
static int mulib_parallel_sh_join_cli_(char **out,int argc,char **argv,int from,int to){
  char *s=NULL;size_t len=0,cap=0;(void)argc;
  for(int i=from;i<to;i++){
    if(i>from&&!mulib_parallel_sh_put_(&s,&len,&cap," ",1))goto bad;
    if(!mulib_parallel_sh_put_(&s,&len,&cap,argv[i],strlen(argv[i])))goto bad;
  }
  if(!s)s=mulib_parallel_sh_dup_("");
  *out=s;return s!=NULL;
bad:free(s);return 0;
}
static int mulib_parallel_sh_num_(const char *text,size_t *n){
  char *end;unsigned long long v;
  if(!text||!*text)return 0;
  for(const char *s=text;*s;s++)if(*s<'0'||*s>'9')return 0;
  errno=0;v=strtoull(text,&end,10);
  if(errno||*end||v>SIZE_MAX)return 0;
  *n=(size_t)v;return 1;
}
int mulib_parallel_sh_main(int argc,char **argv){
  mulib_parallel_sh_t p;size_t jobs=0;int k=0,dry=0,stop=0,retries=0,delim='\n';
  int i=1,start=0,marker=-1,rc=0;char *pattern=NULL,*lua=NULL,*lua_code=NULL;
  const char *file=NULL,*shell=NULL,*joblog_path=NULL;char error[256]={0};
  if(argc<1||!argv)return 2;
  for(int q=0;q<argc;q++)if(!argv[q])return 2;
  while(i<argc){const char *a=argv[i];size_t z;
    if(!strcmp(a,"--")){i++;break;}
    if(!strcmp(a,"-k")||!strcmp(a,"--keep-order")){k=1;i++;continue;}
    if(!strcmp(a,"--dry-run")){dry=1;i++;continue;}
    if(!strcmp(a,"-0")||!strcmp(a,"--null")){delim=0;i++;continue;}
    if(!strcmp(a,"-j")||!strcmp(a,"--jobs")){if(++i>=argc||!mulib_parallel_sh_num_(argv[i],&jobs))goto usage;i++;continue;}
    if(!strncmp(a,"-j",2)&&a[2]){if(!mulib_parallel_sh_num_(a+2,&jobs))goto usage;i++;continue;}
    if(!strncmp(a,"--jobs=",7)){if(!mulib_parallel_sh_num_(a+7,&jobs))goto usage;i++;continue;}
    if(!strcmp(a,"-a")||!strcmp(a,"--arg-file")){if(++i>=argc)goto usage;file=argv[i++];continue;}
    if(!strcmp(a,"--lua")){if(++i>=argc)goto usage;lua=argv[i++];continue;}
    if(!strcmp(a,"--lua-code")){if(++i>=argc)goto usage;lua_code=argv[i++];continue;}
    if(!strcmp(a,"--shell")){if(++i>=argc)goto usage;shell=argv[i++];continue;}
    if(!strcmp(a,"--joblog")){if(++i>=argc)goto usage;joblog_path=argv[i++];continue;}
    if(!strncmp(a,"--joblog=",9)){joblog_path=a+9;i++;continue;}
    if(!strcmp(a,"--retries")){if(++i>=argc||!mulib_parallel_sh_num_(argv[i],&z)||z>INT_MAX)goto usage;retries=(int)z;i++;continue;}
    if(!strncmp(a,"--retries=",10)){if(!mulib_parallel_sh_num_(a+10,&z)||z>INT_MAX)goto usage;retries=(int)z;i++;continue;}
    if(!strcmp(a,"--halt")){if(++i>=argc||strcmp(argv[i],"soon,fail=1"))goto usage;
      stop=1;i++;continue;}
    if(!strcmp(a,"--help")){fprintf(stdout,"parallel-sh: -j N -k -0 -a FILE --shell PROGRAM --joblog FILE --dry-run\n  --retries N --halt soon,fail=1 --lua FILE --lua-code SOURCE\n  [COMMAND ::: ARGS | COMMAND :::: FILE | COMMAND < INPUT]\n");return 0;}
    if(a[0]=='-'&&a[1])goto usage;
    break;
  }
  start=i;
  for(int q=i;q<argc;q++)if(!strcmp(argv[q],":::")||!strcmp(argv[q],"::::")){marker=q;break;}
  if((lua||lua_code)&&(start<argc||file))goto usage;
  if(file&&marker>=0)goto usage;
  if(marker>=0){if(!mulib_parallel_sh_join_cli_(&pattern,argc,argv,start,marker))return 2;}
  else if(!lua&&!lua_code){if(!mulib_parallel_sh_join_cli_(&pattern,argc,argv,start,argc))return 2;}
  if(pattern&&!*pattern){free(pattern);goto usage;}
  if(mulib_parallel_sh_init(&p,jobs))goto error_out;
  p.keep_order=k;p.dry_run=dry;p.halt_on_error=stop;p.retries=retries;if(shell)p.shell=shell;
  if(joblog_path){p.joblog=fopen(joblog_path,"wb");if(!p.joblog)goto fail;
    fputs("Seq\tExit\tAttempts\tCommand\n",p.joblog);
  }
  if(marker>=0){
    for(i=marker;i<argc;i++){
      if(!strcmp(argv[i],":::")){while(++i<argc&&strcmp(argv[i],":::")&&strcmp(argv[i],"::::")){
          rc=mulib_parallel_sh_add_input(&p,pattern,argv[i]);if(rc)goto clean;
        }i--;
      }else if(!strcmp(argv[i],"::::")){FILE *f;
        if(++i>=argc)goto fail;
        f=!strcmp(argv[i],"-")?stdin:fopen(argv[i],"rb");if(!f)goto fail;
        rc=mulib_parallel_sh_read_inputs_(&p,pattern,f,delim);
        if(f!=stdin)fclose(f);
        if(rc)goto clean;
      }else goto fail;
    }
  }else if(file){FILE *f=!strcmp(file,"-")?stdin:fopen(file,"rb");if(!f)goto fail;
    rc=mulib_parallel_sh_read_inputs_(&p,pattern,f,delim);if(f!=stdin)fclose(f);
        if(rc)goto clean;
  }else if(pattern&&*pattern){rc=mulib_parallel_sh_read_inputs_(&p,pattern,stdin,delim);if(rc)goto clean;}
  if(lua){rc=mulib_parallel_sh_lua_file(&p,lua,error,sizeof error);if(rc){fprintf(stderr,"parallel-sh lua: %s\n",error);goto clean;}}
  if(lua_code){rc=mulib_parallel_sh_lua(&p,lua_code,strlen(lua_code),error,sizeof error);if(rc){fprintf(stderr,"parallel-sh lua: %s\n",error);goto clean;}}
  rc=p.ran?p.last_status:mulib_parallel_sh_run(&p);
 clean:if(joblog_path&&p.joblog&&fclose(p.joblog))rc=MULIB_PARALLEL_SH_SYSTEM;
  mulib_parallel_sh_free(&p);free(pattern);return rc==0?0:1;
fail:rc=MULIB_PARALLEL_SH_INVALID;goto clean;
error_out:free(pattern);return 2;
usage:fprintf(stderr,"parallel-sh: unsupported/invalid option; see --help\n");return 2;
}
#endif
