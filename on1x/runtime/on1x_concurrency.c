#define _POSIX_C_SOURCE 200809L
#include "on1x_concurrency.h"
#include <mulib-channel.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
extern char **environ;
#if !defined(ON1X_CSP_ABI_VERSION)
#error "On1x cooperative fibers currently require Linux/glibc x86_64"
#endif
#ifndef ON1X_CSP_MAX_TASKS
#define ON1X_CSP_MAX_TASKS 256
#endif
#ifndef ON1X_CSP_MAX_CHANNELS
#define ON1X_CSP_MAX_CHANNELS 256
#endif
#ifndef ON1X_CSP_MAX_PROCESSES
#define ON1X_CSP_MAX_PROCESSES 128
#endif
#ifndef ON1X_CSP_STREAM_CAPACITY
#define ON1X_CSP_STREAM_CAPACITY 256
#endif
#if ON1X_CSP_STREAM_CAPACITY < 0 || ON1X_CSP_STREAM_CAPACITY > 256
#error "ON1X_CSP_STREAM_CAPACITY must be in 0..256"
#endif
#ifndef ON1X_CSP_MAX_GROUPS
#define ON1X_CSP_MAX_GROUPS 64
#endif
#ifndef ON1X_CSP_MAX_WORK
#define ON1X_CSP_MAX_WORK 1000000
#endif
#ifndef ON1X_CSP_MAX_CALL_DEPTH
#define ON1X_CSP_MAX_CALL_DEPTH 256
#endif
#ifndef ON1X_CSP_STACK_BYTES
#define ON1X_CSP_STACK_BYTES (1024 * 1024)
#endif
#ifndef ON1X_CSP_CALLOC
#define ON1X_CSP_CALLOC calloc
#endif
#ifndef ON1X_CSP_FREE
#define ON1X_CSP_FREE free
#endif
typedef struct {const char *file;size_t line,column;} source_point;
struct on1x_csp_task {
 on1x_csp_task *next,*sibling;
 on1x_csp_group *owner,*group;
 mulib_fiber_t *fiber,*joiner;
 on1x_csp_callback invoke;
 on1x_env *environment;
 source_point at;
 int64_t result;
 size_t calls;
  mulib_channel_t *cancel_signal;
  unsigned shield;
  bool done,joined,waiting,started,cancel_before_start,cancel_requested;
};
struct on1x_csp_group {
 on1x_csp_group *parent;
 on1x_csp_task *owner,*children;
  on1x_csp_channel *channels;
  on1x_csp_process *processes;
 source_point at;
};
struct on1x_csp_channel {
 on1x_csp_channel *next,*sibling;
 mulib_channel_t *channel;
 unsigned role;
};
enum {ON1X_CHANNEL_I64,ON1X_CHANNEL_PROCESS_INPUT,ON1X_CHANNEL_PROCESS_OUTPUT,ON1X_CHANNEL_PROCESS_ERROR};
struct on1x_csp_process {
  on1x_csp_process *next,*sibling;
  on1x_csp_group *owner;
  on1x_csp_task *parent;
  mulib_fiber_t *joiner;
  source_point at;
  pid_t pid;
  int64_t status;
  uint64_t kill_at;
  int input_fd,output_fd,error_fd;
  on1x_csp_channel *input,*output,*error;
  uint8_t input_byte,output_byte,error_byte;
  bool input_pending,output_pending,error_pending;
  bool done,joined,stopping,killed;
};
typedef struct on1x_csp_timer {
 struct on1x_csp_timer *next;
 mulib_channel_t *signal;
 uint64_t expires;
 bool fired;
} on1x_csp_timer;
typedef struct {
 mulib_fiber_scheduler_t *scheduler;
 on1x_csp_task *tasks;
 on1x_csp_channel *channels;
  on1x_csp_process *processes;
 on1x_csp_timer *timers;
 size_t task_count,channel_count,process_count,process_active,io_active,groups,active,work;
 uint64_t progress;
} execution;
static _Thread_local execution *current_execution;
static void request_cancel(on1x_csp_task *);
static bool cancel_owned(on1x_csp_task *,source_point);
static int64_t finish_process(on1x_csp_process *,on1x_csp_task *);
static void reap(on1x_csp_process *);
static uint64_t monotonic_ms(source_point);
static _Noreturn void failure(source_point at,const char *what){
  /* Terminal failures cannot unwind generated frames. Do not leave a known
   * outstanding external child running after this process exits. */
  if(current_execution){
   on1x_csp_process *p;
   for(p=current_execution->processes;p;p=p->next)if(!p->done)(void)kill(-p->pid,SIGKILL);
  }
  fprintf(stderr,"%s:%zu:%zu: ON1X-CSP-RUNTIME: %s\n",at.file,at.line,at.column,what);
 exit(73);
}
static on1x_csp_task *current(source_point at){
 on1x_csp_task *t;mulib_fiber_t *fiber=mulib_fiber_current();
 if(current_execution&&fiber)for(t=current_execution->tasks;t;t=t->next)if(t->fiber==fiber&&!t->done)return t;
 failure(at,"operation outside a compiled On1x fiber");
}
static void tick(source_point at){
 if(!current_execution||current_execution->work++>=ON1X_CSP_MAX_WORK)failure(at,"scheduler work budget exhausted");
 ++current_execution->progress;
}
bool on1x_csp_cancelled(void){
 source_point at={"<cancellation>",1,1};on1x_csp_task *t=current(at);
 return t->cancel_requested&&!t->shield;
}
void on1x_csp_shield_enter(void){
 source_point at={"<cleanup>",1,1};on1x_csp_task *t=current(at);
 if(t->shield==UINT_MAX)failure(at,"cleanup shield depth exhausted");
 ++t->shield;
}
void on1x_csp_shield_leave(void){
 source_point at={"<cleanup>",1,1};on1x_csp_task *t=current(at);
 if(!t->shield)failure(at,"unbalanced cleanup shield");
 --t->shield;
}
static void request_cancel(on1x_csp_task *t){
 if(t->done||t->cancel_requested)return;
 t->cancel_requested=true;
 if(!t->started)t->cancel_before_start=true;
 (void)mulib_channel_close(t->cancel_signal);
 mulib_fiber_unpark(t->fiber);
 ++current_execution->progress;
}
static on1x_csp_channel *channel(on1x_csp_channel *value,source_point at){
 on1x_csp_channel *c;
 (void)current(at);
 for(c=current_execution->channels;c;c=c->next)if(c==value&&c->channel)return c;
 failure(at,"stale or invalid channel");
}
static on1x_csp_channel *channel_new(size_t size,size_t capacity,unsigned role){
 on1x_csp_channel *c=(on1x_csp_channel *)ON1X_CSP_CALLOC(1,sizeof(*c));
 if(!c)return NULL;
 if(mulib_channel_create(&c->channel,size,capacity)!=MULIB_CHANNEL_OK){ON1X_CSP_FREE(c);return NULL;}
 c->role=role;return c;
}
static void channel_discard(on1x_csp_channel *c){
 if(!c)return;
 if(c->channel){(void)mulib_channel_close(c->channel);(void)mulib_channel_destroy(c->channel);}
 ON1X_CSP_FREE(c);
}
static void channel_link(on1x_csp_channel *c,on1x_csp_group *g){
 c->next=current_execution->channels;current_execution->channels=c;++current_execution->channel_count;
 c->sibling=g->channels;g->channels=c;
}
static void process_fd_close(int *fd){
 if(*fd>=0){(void)close(*fd);*fd=-1;if(current_execution->io_active)--current_execution->io_active;++current_execution->progress;}
}
static void process_streams_close(on1x_csp_process *p){
 process_fd_close(&p->input_fd);process_fd_close(&p->output_fd);process_fd_close(&p->error_fd);
 if(p->input&&p->input->channel)(void)mulib_channel_close(p->input->channel);
 if(p->output&&p->output->channel)(void)mulib_channel_close(p->output->channel);
 if(p->error&&p->error->channel)(void)mulib_channel_close(p->error->channel);
}
on1x_csp_group *on1x_csp_group_enter(const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *t=current(at);on1x_csp_group *g;
 tick(at);
 if(current_execution->groups>=ON1X_CSP_MAX_GROUPS)failure(at,"live taskgroup budget exhausted");
 g=(on1x_csp_group *)ON1X_CSP_CALLOC(1,sizeof(*g));
 if(!g)failure(at,"taskgroup allocation failure");
 g->owner=t;g->parent=t->group;g->at=at;t->group=g;++current_execution->groups;
 return g;
}
void on1x_csp_group_finish(on1x_csp_group *g){
 source_point at={"<taskgroup>",1,1};on1x_csp_task *t=current(at),*child;on1x_csp_channel *c;on1x_csp_process *p;
 if(!g||t->group!=g)failure(at,"invalid taskgroup cleanup order");
  at=g->at;tick(at);
 if(t->cancel_requested){
  on1x_csp_shield_enter();
  for(child=g->children;child;child=child->sibling)request_cancel(child);
  for(child=g->children;child;child=child->sibling)if(!child->joined)(void)cancel_owned(child,at);
  for(p=g->processes;p;p=p->sibling)if(!p->joined){
   (void)on1x_csp_process_cancel(p,at.file,at.line,at.column);
  }
  on1x_csp_shield_leave();
 }
 for(child=g->children;child;child=child->sibling)
  if(!child->done||!child->joined)failure(at,"taskgroup exited with an unjoined child");
 for(p=g->processes;p;p=p->sibling){
  if(!p->done||!p->joined)failure(at,"taskgroup exited with an unjoined external process");
  process_streams_close(p);
 }
 for(c=g->channels;c;c=c->sibling){
  int rc=mulib_channel_close(c->channel);
  if(rc!=MULIB_CHANNEL_OK&&rc!=MULIB_CHANNEL_CLOSED)failure(at,"channel close failure during cleanup");
  if(mulib_channel_destroy(c->channel)!=MULIB_CHANNEL_OK)failure(at,"channel still borrowed during cleanup");
  c->channel=NULL;
 }
 t->group=g->parent;--current_execution->groups;ON1X_CSP_FREE(g);
}
static void task_entry(void *arg){
  on1x_csp_task *t=(on1x_csp_task *)arg;
  t->started=true;
  if(!t->cancel_before_start){
   on1x_csp_group *g=on1x_csp_group_enter(t->at.file,t->at.line,t->at.column);
   t->result=t->invoke(t->environment);
   on1x_csp_group_finish(g);
  }
  if(t->calls||t->shield)failure(t->at,"unbalanced generated function cleanup");
 on1x_env_release(t->environment);t->environment=NULL;
 t->done=true;--current_execution->active;++current_execution->progress;
 if(t->joiner)mulib_fiber_unpark(t->joiner);
}
static on1x_csp_task *spawn(on1x_csp_callback invoke,on1x_env *env,on1x_csp_group *owner,source_point at){
 on1x_csp_task *t;execution *e=current_execution;
 tick(at);
 if(!invoke)failure(at,"invalid fiber callback");
 if(e->task_count>=ON1X_CSP_MAX_TASKS)failure(at,"total fiber budget exhausted");
  t=(on1x_csp_task *)ON1X_CSP_CALLOC(1,sizeof(*t));if(!t)failure(at,"fiber record allocation failure");
 if(mulib_channel_create(&t->cancel_signal,1,0)!=MULIB_CHANNEL_OK){ON1X_CSP_FREE(t);failure(at,"cancellation signal allocation failure");}
 t->invoke=invoke;t->environment=env;t->owner=owner;t->at=at;on1x_env_retain(env);
 if(mulib_fiber_spawn(e->scheduler,task_entry,t,ON1X_CSP_STACK_BYTES,&t->fiber)!=MULIB_FIBER_OK)failure(at,"fiber stack/context allocation failure");
 t->next=e->tasks;e->tasks=t;++e->task_count;++e->active;
 if(owner){t->sibling=owner->children;owner->children=t;}
 return t;
}
on1x_csp_task *on1x_csp_launch(on1x_csp_callback invoke,on1x_env *env,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *t=current(at);
 if(!t->group)failure(at,"launch outside taskgroup");
 return spawn(invoke,env,t->group,at);
}
static bool cancel_owned(on1x_csp_task *value,source_point at){
 on1x_csp_task *caller=current(at),*t;bool requested;
 tick(at);
 for(t=current_execution->tasks;t&&t!=value;t=t->next){}
 if(!t||t==caller||t->joined||!t->owner||t->owner->owner!=caller||t->joiner)
  failure(at,"invalid, repeated or non-owning internal cancellation");
 requested=!t->done;request_cancel(t);t->joiner=caller->fiber;caller->waiting=true;
 while(!t->done)(void)mulib_fiber_park_until(MULIB_XWAIT_FOREVER);
 mulib_fiber_clear_wake();caller->waiting=false;t->joined=true;
 ++current_execution->progress;return requested;
}
bool on1x_csp_cancel(on1x_csp_task *value,const char *file,size_t line,size_t column){
 source_point at={file,line,column};return cancel_owned(value,at);
}
bool on1x_csp_cancel_ready(on1x_csp_task *value,const char *file,size_t line,size_t column){
 return on1x_csp_cancel(value,file,line,column);
}
static uint64_t monotonic_ms(source_point at){
 struct timespec now;if(clock_gettime(CLOCK_MONOTONIC,&now)!=0)failure(at,"monotonic clock unavailable");
 return (uint64_t)now.tv_sec*1000u+(uint64_t)now.tv_nsec/1000000u;
}
/* Control offers use ordinary MuLIB channels, preserving atomic rendezvous and
 * losing-offer withdrawal without changing the kernel's UTC timeout semantics. */
static int select_controlled(mulib_channel_case_t *cases,size_t count,bool fallback,
                            int64_t delay,size_t *selected,source_point at){
 on1x_csp_task *t=current(at);on1x_csp_timer timer={0};uint8_t signal=0;
 size_t total=count,cancel_index=SIZE_MAX,timer_index=SIZE_MAX;int rc;
 if(delay<-1)failure(at,"negative poll duration");
 if(on1x_csp_cancelled()){*selected=SIZE_MAX;return MULIB_CHANNEL_TIMEOUT;}
 if(delay>=0){
  uint64_t now=monotonic_ms(at);
  if((uint64_t)delay>UINT64_MAX-now)failure(at,"poll deadline overflow");
  if(mulib_channel_create(&timer.signal,1,0)!=MULIB_CHANNEL_OK)failure(at,"poll timer allocation failure");
  timer.expires=now+(uint64_t)delay;timer.next=current_execution->timers;current_execution->timers=&timer;
  if(!delay){timer.fired=true;(void)mulib_channel_close(timer.signal);}
  timer_index=total;cases[total++]=mulib_channel_receive_case(mulib_channel_receiver(timer.signal),&signal);
 }
 if(!t->shield){cancel_index=total;cases[total++]=mulib_channel_receive_case(mulib_channel_receiver(t->cancel_signal),&signal);}
 t->waiting=true;
 rc=mulib_channel_select(cases,total,fallback,MULIB_XWAIT_FOREVER,selected);
 t->waiting=false;++current_execution->progress;
 if(timer.signal){
  on1x_csp_timer **link=&current_execution->timers;
  while(*link&&*link!=&timer)link=&(*link)->next;
  if(!*link)failure(at,"lost poll timer registration");
  *link=timer.next;
  if(mulib_channel_destroy(timer.signal)!=MULIB_CHANNEL_OK)failure(at,"live poll timer observer");
 }
 if(*selected==cancel_index&&cancel_index!=SIZE_MAX){*selected=SIZE_MAX;return MULIB_CHANNEL_TIMEOUT;}
 if(*selected==timer_index&&timer_index!=SIZE_MAX){*selected=count;return MULIB_CHANNEL_TIMEOUT;}
 if(rc==MULIB_CHANNEL_DEFAULT)*selected=count+(delay>=0?1u:0u);
 return rc;
}
static void reap(on1x_csp_process *p){
 int status;pid_t observed;
 if(p->done)return;
 do{observed=waitpid(p->pid,&status,WNOHANG);}while(observed<0&&errno==EINTR);
 if(observed<0)failure(p->at,"external waitpid failure");
 if(observed==0)return;
 p->done=true;--current_execution->process_active;++current_execution->progress;
 p->status=WIFEXITED(status)?WEXITSTATUS(status):WIFSIGNALED(status)?-WTERMSIG(status):-1;
 if(p->input){process_fd_close(&p->input_fd);if(p->input->channel)(void)mulib_channel_close(p->input->channel);}
 if(p->joiner)mulib_fiber_unpark(p->joiner);
}
static int spawn_attributes(posix_spawnattr_t *attr){
 sigset_t defaults;int rc=posix_spawnattr_init(attr);
 if(rc)return rc;
 if(sigemptyset(&defaults)||sigaddset(&defaults,SIGPIPE)){(void)posix_spawnattr_destroy(attr);return errno?errno:EINVAL;}
 rc=posix_spawnattr_setflags(attr,POSIX_SPAWN_SETPGROUP|POSIX_SPAWN_SETSIGDEF);
 if(!rc)rc=posix_spawnattr_setpgroup(attr,0);
 if(!rc)rc=posix_spawnattr_setsigdefault(attr,&defaults);
 if(rc)(void)posix_spawnattr_destroy(attr);
 return rc;
}
static int capture_pipe(int fds[2],int parent){
 int flags;
 if(pipe(fds)!=0)return errno?errno:EIO;
 if(fds[0]<=STDERR_FILENO||fds[1]<=STDERR_FILENO){(void)close(fds[0]);(void)close(fds[1]);fds[0]=fds[1]=-1;return EINVAL;}
 if(fcntl(fds[0],F_SETFD,FD_CLOEXEC)<0||fcntl(fds[1],F_SETFD,FD_CLOEXEC)<0){int rc=errno;(void)close(fds[0]);(void)close(fds[1]);fds[0]=fds[1]=-1;return rc;}
 flags=fcntl(fds[parent],F_GETFL,0);
 if(flags<0||fcntl(fds[parent],F_SETFL,flags|O_NONBLOCK)<0){int rc=errno;(void)close(fds[0]);(void)close(fds[1]);fds[0]=fds[1]=-1;return rc;}
 return 0;
}
on1x_csp_process *on1x_csp_launch_exec(const char *const *argv,size_t count,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *t=current(at);on1x_csp_process *p;
 posix_spawnattr_t attr;pid_t pid;int rc;size_t i,total=0;
 tick(at);
 if(!t->group||!argv||count<1||count>9||!argv[0]||argv[0][0]!='/'||argv[count])failure(at,"external launch requires absolute path and 1..9 argv items");
 if(current_execution->process_count>=ON1X_CSP_MAX_PROCESSES)failure(at,"external process budget exhausted");
 for(i=0;i<count;++i){
  size_t length;if(!argv[i]||(length=strnlen(argv[i],4097))>4096)failure(at,"invalid or oversized external argument");
  if(length+1>16384-total)failure(at,"external command byte budget exhausted");
  total+=length+1;
 }
 p=(on1x_csp_process *)ON1X_CSP_CALLOC(1,sizeof(*p));if(!p)failure(at,"external process record allocation failure");
 p->input_fd=p->output_fd=p->error_fd=-1;
 rc=spawn_attributes(&attr);if(rc){ON1X_CSP_FREE(p);failure(at,"external spawn attribute allocation failure");}
 if(!rc)rc=posix_spawn(&pid,argv[0],NULL,&attr,(char *const *)argv,environ);
 (void)posix_spawnattr_destroy(&attr);
 if(rc){ON1X_CSP_FREE(p);failure(at,"external posix_spawn failure");}
 p->pid=pid;p->parent=t;p->owner=t->group;p->at=at;
 p->next=current_execution->processes;current_execution->processes=p;
 p->sibling=t->group->processes;t->group->processes=p;
 ++current_execution->process_count;++current_execution->process_active;return p;
}
void on1x_csp_launch_capture(const char *const *argv,size_t count,on1x_csp_process **process,
 on1x_csp_channel **input,on1x_csp_channel **output,on1x_csp_channel **error,
 const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *t=current(at);on1x_csp_process *p=NULL;
 on1x_csp_channel *in=NULL,*out=NULL,*err=NULL;posix_spawnattr_t attr;posix_spawn_file_actions_t actions;
 int inpipe[2]={-1,-1},outpipe[2]={-1,-1},errpipe[2]={-1,-1};int rc;pid_t pid;size_t i,total=0;
 bool have_attr=false,have_actions=false;const char *failure_text="captured process setup failure";
 tick(at);
 if(process)*process=NULL;
 if(input)*input=NULL;
 if(output)*output=NULL;
 if(error)*error=NULL;
 if(!process||!input||!output||!error||!t->group||!argv||count<1||count>9||!argv[0]||argv[0][0]!='/'||argv[count])failure(at,"captured launch requires absolute path and 1..9 argv items");
 if(current_execution->process_count>=ON1X_CSP_MAX_PROCESSES)failure(at,"external process budget exhausted");
 if((size_t)ON1X_CSP_MAX_CHANNELS<3||current_execution->channel_count>(size_t)ON1X_CSP_MAX_CHANNELS-3)failure(at,"captured stream channel budget exhausted");
 for(i=0;i<count;++i){
  size_t length;if(!argv[i]||(length=strnlen(argv[i],4097))>4096)failure(at,"invalid or oversized captured argument");
  if(length+1>16384-total)failure(at,"captured command byte budget exhausted");
  total+=length+1;
 }
 p=(on1x_csp_process *)ON1X_CSP_CALLOC(1,sizeof(*p));
 in=channel_new(sizeof(uint8_t),ON1X_CSP_STREAM_CAPACITY,ON1X_CHANNEL_PROCESS_INPUT);
 out=channel_new(sizeof(uint8_t),ON1X_CSP_STREAM_CAPACITY,ON1X_CHANNEL_PROCESS_OUTPUT);
 err=channel_new(sizeof(uint8_t),ON1X_CSP_STREAM_CAPACITY,ON1X_CHANNEL_PROCESS_ERROR);
 if(!p||!in||!out||!err){failure_text="captured process/channel allocation failure";goto failed;}
 p->input_fd=p->output_fd=p->error_fd=-1;p->input=in;p->output=out;p->error=err;
 if(capture_pipe(inpipe,1)||capture_pipe(outpipe,0)||capture_pipe(errpipe,0)){failure_text="captured pipe allocation failure";goto failed;}
 rc=spawn_attributes(&attr);if(rc){failure_text="captured spawn attribute failure";goto failed;}have_attr=true;
 rc=posix_spawn_file_actions_init(&actions);if(rc){failure_text="captured spawn actions failure";goto failed;}have_actions=true;
 rc=posix_spawn_file_actions_adddup2(&actions,inpipe[0],STDIN_FILENO);
 if(!rc)rc=posix_spawn_file_actions_adddup2(&actions,outpipe[1],STDOUT_FILENO);
 if(!rc)rc=posix_spawn_file_actions_adddup2(&actions,errpipe[1],STDERR_FILENO);
 for(i=0;!rc&&i<2;++i)rc=posix_spawn_file_actions_addclose(&actions,inpipe[i]);
 for(i=0;!rc&&i<2;++i)rc=posix_spawn_file_actions_addclose(&actions,outpipe[i]);
 for(i=0;!rc&&i<2;++i)rc=posix_spawn_file_actions_addclose(&actions,errpipe[i]);
 if(rc){failure_text="captured spawn action construction failure";goto failed;}
 rc=posix_spawn(&pid,argv[0],&actions,&attr,(char *const *)argv,environ);
 if(rc){failure_text="captured posix_spawn failure";goto failed;}
 (void)posix_spawn_file_actions_destroy(&actions);have_actions=false;
 (void)posix_spawnattr_destroy(&attr);have_attr=false;
 (void)close(inpipe[0]);inpipe[0]=-1;(void)close(outpipe[1]);outpipe[1]=-1;(void)close(errpipe[1]);errpipe[1]=-1;
 p->pid=pid;p->parent=t;p->owner=t->group;p->at=at;
 p->input_fd=inpipe[1];p->output_fd=outpipe[0];p->error_fd=errpipe[0];
 inpipe[1]=outpipe[0]=errpipe[0]=-1;current_execution->io_active+=3;
 channel_link(in,t->group);channel_link(out,t->group);channel_link(err,t->group);
 p->next=current_execution->processes;current_execution->processes=p;
 p->sibling=t->group->processes;t->group->processes=p;
 ++current_execution->process_count;++current_execution->process_active;
 *process=p;*input=in;*output=out;*error=err;return;
failed:
 if(have_actions)(void)posix_spawn_file_actions_destroy(&actions);
 if(have_attr)(void)posix_spawnattr_destroy(&attr);
 for(i=0;i<2;++i){if(inpipe[i]>=0)(void)close(inpipe[i]);if(outpipe[i]>=0)(void)close(outpipe[i]);if(errpipe[i]>=0)(void)close(errpipe[i]);}
 channel_discard(in);channel_discard(out);channel_discard(err);ON1X_CSP_FREE(p);failure(at,failure_text);
}
/* POSIX pipes have no MSG_NOSIGNAL equivalent. Block SIGPIPE only across this
 * nonblocking write, consume the signal generated by our EPIPE (but never a
 * pre-existing pending signal), then restore the fiber thread's exact mask. */
static ssize_t pipe_write_no_sigpipe(on1x_csp_process *p,const void *data,size_t size){
 sigset_t blocked,previous,pending;struct timespec none={0,0};ssize_t wrote;int rc,saved,member,observed;
 if(sigemptyset(&blocked)||sigaddset(&blocked,SIGPIPE))failure(p->at,"cannot construct captured-pipe SIGPIPE mask");
 rc=pthread_sigmask(SIG_BLOCK,&blocked,&previous);if(rc)failure(p->at,"cannot block captured-pipe SIGPIPE");
 if(sigpending(&pending)!=0)failure(p->at,"cannot inspect captured-pipe SIGPIPE state");
 member=sigismember(&pending,SIGPIPE);if(member<0)failure(p->at,"cannot inspect captured-pipe SIGPIPE membership");
 do{wrote=write(p->input_fd,data,size);}while(wrote<0&&errno==EINTR);
 saved=errno;
 if(wrote<0&&saved==EPIPE&&!member){
  do{observed=sigtimedwait(&blocked,NULL,&none);}while(observed<0&&errno==EINTR);
  if(observed<0&&errno!=EAGAIN)failure(p->at,"cannot consume captured-pipe SIGPIPE");
 }
 rc=pthread_sigmask(SIG_SETMASK,&previous,NULL);if(rc)failure(p->at,"cannot restore captured-pipe signal mask");
 errno=saved;return wrote;
}
static void pump_process_input(on1x_csp_process *p){
 int rc;ssize_t wrote;
 if(!p->input||p->input_fd<0)return;
 if(!p->input_pending){
  rc=mulib_channel_receive(mulib_channel_receiver(p->input->channel),&p->input_byte,0);
  if(rc==MULIB_CHANNEL_OK){p->input_pending=true;++current_execution->progress;}
  else if(rc==MULIB_CHANNEL_CLOSED){process_fd_close(&p->input_fd);return;}
  else if(rc!=MULIB_CHANNEL_TIMEOUT)failure(p->at,"captured stdin channel failure");
 }
 if(!p->input_pending||p->input_fd<0)return;
 wrote=pipe_write_no_sigpipe(p,&p->input_byte,1);
 if(wrote==1){p->input_pending=false;++current_execution->progress;}
 else if(wrote==0)failure(p->at,"captured stdin pipe made no progress");
 else if(wrote<0&&(errno==EPIPE)){p->input_pending=false;process_fd_close(&p->input_fd);(void)mulib_channel_close(p->input->channel);}
 else if(wrote<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK)failure(p->at,"captured stdin pipe failure");
}
static void pump_process_output(on1x_csp_process *p,int *fd,on1x_csp_channel *stream,uint8_t *byte,bool *pending,const char *what){
 int rc;ssize_t got;
 if(!stream||*fd<0)return;
 if(*pending){
  rc=mulib_channel_send(mulib_channel_sender(stream->channel),byte,0);
  if(rc==MULIB_CHANNEL_OK){*pending=false;++current_execution->progress;}
  else if(rc==MULIB_CHANNEL_CLOSED){*pending=false;process_fd_close(fd);return;}
  else if(rc!=MULIB_CHANNEL_TIMEOUT)failure(p->at,what);
 }
 if(*pending||*fd<0)return;
 do{got=read(*fd,byte,1);}while(got<0&&errno==EINTR);
 if(got==1){*pending=true;++current_execution->progress;}
 else if(got==0){process_fd_close(fd);(void)mulib_channel_close(stream->channel);}
 else if(got<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK)failure(p->at,what);
}
static void pump_process(on1x_csp_process *p){
 pump_process_input(p);
 pump_process_output(p,&p->output_fd,p->output,&p->output_byte,&p->output_pending,"captured stdout pipe/channel failure");
 pump_process_output(p,&p->error_fd,p->error,&p->error_byte,&p->error_pending,"captured stderr pipe/channel failure");
}
static on1x_csp_process *owned_process(on1x_csp_process *value,on1x_csp_task *caller,source_point at){
 on1x_csp_process *p;
 for(p=current_execution->processes;p&&p!=value;p=p->next){}
 if(!p||p->joined||!p->owner||p->owner->owner!=caller||p->parent!=caller||p->joiner)failure(at,"invalid, repeated or non-owning external join/cancel");
 return p;
}
static int64_t finish_process(on1x_csp_process *p,on1x_csp_task *caller){
 p->joiner=caller->fiber;caller->waiting=true;
 while(!p->done){
  if(on1x_csp_cancelled()){p->joiner=NULL;caller->waiting=false;mulib_fiber_clear_wake();return 0;}
  (void)mulib_fiber_park_until(MULIB_XWAIT_FOREVER);
 }
 mulib_fiber_clear_wake();caller->waiting=false;p->joined=true;
 ++current_execution->progress;return p->status;
}
int64_t on1x_csp_process_join(on1x_csp_process *value,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *caller=current(at);on1x_csp_process *p;
 tick(at);p=owned_process(value,caller,at);
 if(p->input&&p->input->channel)(void)mulib_channel_close(p->input->channel);
 reap(p);return finish_process(p,caller);
}
bool on1x_csp_process_cancel(on1x_csp_process *value,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *caller=current(at);on1x_csp_process *p;bool requested=false;
 tick(at);p=owned_process(value,caller,at);reap(p);
 if(!p->done){
   if(p->input&&p->input->channel)(void)mulib_channel_close(p->input->channel);
   process_fd_close(&p->input_fd);p->input_pending=false;
  if(kill(-p->pid,SIGTERM)!=0){
   if(errno!=ESRCH)failure(at,"external process-group SIGTERM failure");
   reap(p);
  }else{p->stopping=true;p->kill_at=monotonic_ms(at)+250;requested=true;}
 }
 (void)finish_process(p,caller);return requested;
}
int64_t on1x_csp_join(on1x_csp_task *value,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *caller=current(at),*t;
 tick(at);
 for(t=current_execution->tasks;t&&t!=value;t=t->next){}
 if(!t||t==caller||t->joined||!t->owner||t->owner->owner!=caller||t->joiner)failure(at,"invalid, repeated or non-owning join");
 t->joiner=caller->fiber;caller->waiting=true;
 while(!t->done){
  if(on1x_csp_cancelled()){t->joiner=NULL;caller->waiting=false;mulib_fiber_clear_wake();return 0;}
  (void)mulib_fiber_park_until(MULIB_XWAIT_FOREVER);
 }
 mulib_fiber_clear_wake();caller->waiting=false;t->joined=true;
 ++current_execution->progress;return t->result;
}
on1x_csp_channel *on1x_csp_channel_create(int64_t capacity,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *t=current(at);on1x_csp_channel *c;
 tick(at);
 if(!t->group||capacity<0||capacity>256)failure(at,"invalid channel owner/capacity");
 if(current_execution->channel_count>=ON1X_CSP_MAX_CHANNELS)failure(at,"total channel budget exhausted");
 c=channel_new(sizeof(int64_t),(size_t)capacity,ON1X_CHANNEL_I64);if(!c)failure(at,"channel storage allocation failure");
 channel_link(c,t->group);return c;
}
int64_t on1x_csp_send(on1x_csp_channel *value,int64_t payload,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *t=current(at);on1x_csp_channel *c=channel(value,at);int rc;
 if(c->role!=ON1X_CHANNEL_I64)failure(at,"i64 send used with byte process stream");
 tick(at);t->waiting=true;
 {mulib_channel_case_t cases[2];size_t selected;
  cases[0]=mulib_channel_send_case(mulib_channel_sender(c->channel),&payload);
  rc=select_controlled(cases,1,false,-1,&selected,at);}
 t->waiting=false;++current_execution->progress;
 if(on1x_csp_cancelled())return 0;
 if(rc!=MULIB_CHANNEL_OK&&rc!=MULIB_CHANNEL_CLOSED)failure(at,"channel send allocation/protocol failure");
 return rc==MULIB_CHANNEL_CLOSED?1:0;
}
bool on1x_csp_receive(on1x_csp_channel *value,int64_t *payload,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *t=current(at);on1x_csp_channel *c=channel(value,at);int rc;
 if(!payload)failure(at,"invalid receive output");
 if(c->role!=ON1X_CHANNEL_I64)failure(at,"i64 receive used with byte process stream");
 tick(at);t->waiting=true;
 {mulib_channel_case_t cases[2];size_t selected;
  cases[0]=mulib_channel_receive_case(mulib_channel_receiver(c->channel),payload);
  rc=select_controlled(cases,1,false,-1,&selected,at);}
 t->waiting=false;++current_execution->progress;
 if(on1x_csp_cancelled())return false;
 if(rc!=MULIB_CHANNEL_OK&&rc!=MULIB_CHANNEL_CLOSED)failure(at,"channel receive allocation/protocol failure");
 return rc==MULIB_CHANNEL_OK;
}
bool on1x_csp_close(on1x_csp_channel *value,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_channel *c=channel(value,at);int rc;
 if(c->role!=ON1X_CHANNEL_I64)failure(at,"i64 close used with byte process stream");
 tick(at);rc=mulib_channel_close(c->channel);
 if(rc!=MULIB_CHANNEL_OK&&rc!=MULIB_CHANNEL_CLOSED)failure(at,"invalid close");
 return rc==MULIB_CHANNEL_OK;
}
int64_t on1x_csp_stream_send(on1x_csp_channel *value,uint8_t payload,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *t=current(at);on1x_csp_channel *c=channel(value,at);int rc;
 if(c->role!=ON1X_CHANNEL_PROCESS_INPUT)failure(at,"byte send requires captured stdin sender");
 tick(at);t->waiting=true;
 {mulib_channel_case_t cases[2];size_t selected;
  cases[0]=mulib_channel_send_case(mulib_channel_sender(c->channel),&payload);
  rc=select_controlled(cases,1,false,-1,&selected,at);}
 t->waiting=false;++current_execution->progress;
 if(on1x_csp_cancelled())return 0;
 if(rc!=MULIB_CHANNEL_OK&&rc!=MULIB_CHANNEL_CLOSED)failure(at,"captured stdin channel failure");
 return rc==MULIB_CHANNEL_CLOSED?1:0;
}
bool on1x_csp_stream_receive(on1x_csp_channel *value,uint8_t *payload,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *t=current(at);on1x_csp_channel *c=channel(value,at);int rc;
 if(!payload||c->role==ON1X_CHANNEL_PROCESS_INPUT||c->role==ON1X_CHANNEL_I64)failure(at,"byte receive requires captured stdout/stderr receiver");
 tick(at);t->waiting=true;
 {mulib_channel_case_t cases[2];size_t selected;
  cases[0]=mulib_channel_receive_case(mulib_channel_receiver(c->channel),payload);
  rc=select_controlled(cases,1,false,-1,&selected,at);}
 t->waiting=false;++current_execution->progress;
 if(on1x_csp_cancelled())return false;
 if(rc!=MULIB_CHANNEL_OK&&rc!=MULIB_CHANNEL_CLOSED)failure(at,"captured output channel failure");
 return rc==MULIB_CHANNEL_OK;
}
bool on1x_csp_stream_close(on1x_csp_channel *value,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_channel *c=channel(value,at);int rc;
 if(c->role!=ON1X_CHANNEL_PROCESS_INPUT)failure(at,"only captured stdin sender can be closed");
 tick(at);rc=mulib_channel_close(c->channel);
 if(rc!=MULIB_CHANNEL_OK&&rc!=MULIB_CHANNEL_CLOSED)failure(at,"captured stdin close failure");
 return rc==MULIB_CHANNEL_OK;
}
static void select_offers(on1x_csp_offer *offers,size_t count,bool has_default,int64_t delay,int64_t *payload,size_t *selected,bool *closed,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *t=current(at);
  mulib_channel_case_t cases[18];uint8_t bytes[16]={0};bool byte_offer[16]={false};size_t i,index=SIZE_MAX;int rc;
 if((count&&!offers)||!payload||!selected||!closed||(!count&&delay<0)||count>16)failure(at,"invalid poll offers");
 tick(at);
 for(i=0;i<count;++i){
  on1x_csp_channel *c=channel(offers[i].channel,at);
   byte_offer[i]=c->role!=ON1X_CHANNEL_I64;
   if(offers[i].operation==MULIB_CHANNEL_SEND){
    if(byte_offer[i]){
     if(c->role!=ON1X_CHANNEL_PROCESS_INPUT)failure(at,"poll byte send requires captured stdin sender");
     if(offers[i].payload<0||offers[i].payload>UINT8_MAX)failure(at,"poll byte payload outside u8 range");
     bytes[i]=(uint8_t)offers[i].payload;
    }
    /* Never copy a byte from the representation of an int64_t: the kernel's
     * element size must address an actual object of the matching C type. */
    cases[i]=mulib_channel_send_case(mulib_channel_sender(c->channel),byte_offer[i]?(const void *)&bytes[i]:(const void *)&offers[i].payload);
   }else if(offers[i].operation==MULIB_CHANNEL_RECEIVE){
    if(byte_offer[i]&&c->role!=ON1X_CHANNEL_PROCESS_OUTPUT&&c->role!=ON1X_CHANNEL_PROCESS_ERROR)failure(at,"poll byte receive requires captured stdout/stderr receiver");
    cases[i]=mulib_channel_receive_case(mulib_channel_receiver(c->channel),byte_offer[i]?(void *)&bytes[i]:(void *)&offers[i].payload);
   }
  else failure(at,"invalid poll operation");
 }
 t->waiting=true;
 rc=select_controlled(cases,count,has_default,delay,&index,at);
 t->waiting=false;++current_execution->progress;
 if(on1x_csp_cancelled()||(rc==MULIB_CHANNEL_TIMEOUT&&delay>=0)||(rc==MULIB_CHANNEL_DEFAULT&&has_default)){*selected=index;*payload=0;*closed=false;return;}
 if((rc!=MULIB_CHANNEL_OK&&rc!=MULIB_CHANNEL_CLOSED)||index>=count)failure(at,"poll allocation/protocol failure");
  *selected=index;*closed=rc==MULIB_CHANNEL_CLOSED;
  *payload=rc==MULIB_CHANNEL_OK?(byte_offer[index]?(int64_t)bytes[index]:offers[index].payload):0;
}
void on1x_csp_select(on1x_csp_offer *offers,size_t count,bool fallback,int64_t *payload,size_t *selected,bool *closed,const char *file,size_t line,size_t column){
 select_offers(offers,count,fallback,-1,payload,selected,closed,file,line,column);
}
void on1x_csp_select_after(on1x_csp_offer *offers,size_t count,bool fallback,int64_t delay,int64_t *payload,size_t *selected,bool *closed,const char *file,size_t line,size_t column){
 source_point at={file,line,column};if(delay<0)failure(at,"negative poll duration");
 select_offers(offers,count,fallback,delay,payload,selected,closed,file,line,column);
}
void on1x_csp_yield(const char *file,size_t line,size_t column){
 source_point at={file,line,column};(void)current(at);tick(at);mulib_fiber_yield();
}
void on1x_csp_call_enter(const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *t=current(at);tick(at);
 if(t->calls++>=ON1X_CSP_MAX_CALL_DEPTH)failure(at,"fiber call-depth budget exhausted");
}
void on1x_csp_call_leave(void){
 source_point at={"<function>",1,1};on1x_csp_task *t=current(at);
 if(!t->calls)failure(at,"invalid generated function cleanup");
 --t->calls;
}
static void supervise(void *arg){
 execution *e=(execution *)arg;uint64_t previous=e->progress;unsigned quiet=0;
 while(e->active){
  on1x_csp_task *t,*blocked=NULL;on1x_csp_process *p;bool all_waiting=true,progressed;
  for(p=e->processes;p;p=p->next){
   pump_process(p);reap(p);pump_process(p);
   if(!p->done&&p->stopping&&!p->killed&&monotonic_ms(p->at)>=p->kill_at){
    if(kill(-p->pid,SIGKILL)!=0&&errno!=ESRCH)failure(p->at,"external process-group SIGKILL failure");
    p->killed=true;
   }
  }
  {on1x_csp_timer *timer;for(timer=e->timers;timer;timer=timer->next)if(!timer->fired&&monotonic_ms((source_point){"<timer>",1,1})>=timer->expires){timer->fired=true;(void)mulib_channel_close(timer->signal);++e->progress;}}
  for(t=e->tasks;t;t=t->next)if(!t->done){if(!t->waiting)all_waiting=false;else blocked=t;}
  /* A channel/pipe commit may have made a task runnable while its adapter still
   * reports waiting. Allow complete scheduler rounds for it to acknowledge. */
  if(all_waiting&&previous==e->progress&&!e->process_active&&!e->io_active&&!e->timers){
   if(++quiet>=2)failure(blocked?blocked->at:(source_point){"<scheduler>",1,1},"quiescent fiber/channel deadlock");
  }else quiet=0;
  progressed=previous!=e->progress;previous=e->progress;
  if(all_waiting&&(e->process_active||e->io_active||e->timers)&&!progressed){struct timespec idle={0,1000000};(void)nanosleep(&idle,NULL);mulib_fiber_yield();}
  else mulib_fiber_yield();
 }
}
int64_t on1x_csp_run(on1x_csp_callback invoke,on1x_env *env,const char *file,size_t line,size_t column){
 source_point at={file,line,column};execution e={0};on1x_csp_task *root,*t;on1x_csp_channel *c;on1x_csp_process *p;int64_t result;
 if(current_execution||mulib_fiber_current())failure(at,"nested or foreign scheduler entry");
 current_execution=&e;
 if(mulib_fiber_scheduler_create(&e.scheduler)!=MULIB_FIBER_OK)failure(at,"scheduler allocation failure");
 root=spawn(invoke,env,NULL,at);
 if(mulib_fiber_spawn(e.scheduler,supervise,&e,ON1X_CSP_STACK_BYTES,NULL)!=MULIB_FIBER_OK)failure(at,"supervisor allocation failure");
 if(mulib_fiber_run(e.scheduler)!=MULIB_FIBER_OK)failure(at,"scheduler execution failure");
 if(e.active||e.process_active||e.io_active||e.groups||!root->done)failure(at,"unfinished scheduler cleanup");
 result=root->result;
 if(mulib_fiber_scheduler_destroy(e.scheduler)!=MULIB_FIBER_OK)failure(at,"scheduler destruction failure");
 while((t=e.tasks)!=NULL){e.tasks=t->next;if(mulib_channel_destroy(t->cancel_signal)!=MULIB_CHANNEL_OK)failure(at,"live cancellation observer");ON1X_CSP_FREE(t);}
 while((c=e.channels)!=NULL){e.channels=c->next;if(c->channel)failure(at,"unreleased channel");ON1X_CSP_FREE(c);}
 while((p=e.processes)!=NULL){e.processes=p->next;if(!p->done||!p->joined)failure(at,"unreaped external process");ON1X_CSP_FREE(p);}
 current_execution=NULL;return result;
}
