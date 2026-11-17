#define _POSIX_C_SOURCE 200809L
#include "on1x_concurrency.h"
#include <mulib-channel.h>
#include <errno.h>
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
  bool done,joined,waiting,started,cancel_before_start;
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
};
struct on1x_csp_process {
  on1x_csp_process *next,*sibling;
  on1x_csp_group *owner;
  on1x_csp_task *parent;
  mulib_fiber_t *joiner;
  source_point at;
  pid_t pid;
  int64_t status;
  uint64_t kill_at;
  bool done,joined,stopping,killed;
};
typedef struct {
 mulib_fiber_scheduler_t *scheduler;
 on1x_csp_task *tasks;
  on1x_csp_channel *channels;
  on1x_csp_process *processes;
  size_t task_count,channel_count,process_count,process_active,groups,active,work;
 uint64_t progress;
} execution;
static _Thread_local execution *current_execution;
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
static on1x_csp_channel *channel(on1x_csp_channel *value,source_point at){
 on1x_csp_channel *c;
 (void)current(at);
 for(c=current_execution->channels;c;c=c->next)if(c==value&&c->channel)return c;
 failure(at,"stale or invalid channel");
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
 for(child=g->children;child;child=child->sibling)
  if(!child->done||!child->joined)failure(at,"taskgroup exited with an unjoined child");
 for(p=g->processes;p;p=p->sibling)
  if(!p->done||!p->joined)failure(at,"taskgroup exited with an unjoined external process");
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
 if(t->calls)failure(t->at,"unbalanced generated function cleanup");
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
bool on1x_csp_cancel_ready(on1x_csp_task *value,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *caller=current(at),*t;
 tick(at);
 for(t=current_execution->tasks;t&&t!=value;t=t->next){}
 if(!t||t==caller||t->joined||t->started||t->done||!t->owner||t->owner->owner!=caller||t->joiner)
  failure(at,"internal fiber was not an immediately ready, owned launch");
 t->cancel_before_start=true;t->joiner=caller->fiber;caller->waiting=true;
 while(!t->done)(void)mulib_fiber_park_until(MULIB_XWAIT_FOREVER);
 mulib_fiber_clear_wake();caller->waiting=false;t->joined=true;
 ++current_execution->progress;return true;
}
static uint64_t monotonic_ms(source_point at){
 struct timespec now;if(clock_gettime(CLOCK_MONOTONIC,&now)!=0)failure(at,"monotonic clock unavailable");
 return (uint64_t)now.tv_sec*1000u+(uint64_t)now.tv_nsec/1000000u;
}
static void reap(on1x_csp_process *p){
 int status;pid_t observed;
 if(p->done)return;
 do{observed=waitpid(p->pid,&status,WNOHANG);}while(observed<0&&errno==EINTR);
 if(observed<0)failure(p->at,"external waitpid failure");
 if(observed==0)return;
 p->done=true;--current_execution->process_active;++current_execution->progress;
 p->status=WIFEXITED(status)?WEXITSTATUS(status):WIFSIGNALED(status)?-WTERMSIG(status):-1;
 if(p->joiner)mulib_fiber_unpark(p->joiner);
}
on1x_csp_process *on1x_csp_launch_exec(const char *const *argv,size_t count,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *t=current(at);on1x_csp_process *p;
 posix_spawnattr_t attr;pid_t pid;int rc;size_t i;
 tick(at);
 if(!t->group||!argv||count<1||count>9||!argv[0]||argv[0][0]!='/'||argv[count])failure(at,"external launch requires absolute path and 1..9 argv items");
 if(current_execution->process_count>=ON1X_CSP_MAX_PROCESSES)failure(at,"external process budget exhausted");
 for(i=0;i<count;++i)if(!argv[i]||strnlen(argv[i],4097)>4096)failure(at,"invalid or oversized external argument");
 p=(on1x_csp_process *)ON1X_CSP_CALLOC(1,sizeof(*p));if(!p)failure(at,"external process record allocation failure");
 rc=posix_spawnattr_init(&attr);if(rc){ON1X_CSP_FREE(p);failure(at,"external spawn attribute allocation failure");}
 rc=posix_spawnattr_setflags(&attr,POSIX_SPAWN_SETPGROUP);
 if(!rc)rc=posix_spawnattr_setpgroup(&attr,0);
 if(!rc)rc=posix_spawn(&pid,argv[0],NULL,&attr,(char *const *)argv,environ);
 (void)posix_spawnattr_destroy(&attr);
 if(rc){ON1X_CSP_FREE(p);failure(at,"external posix_spawn failure");}
 p->pid=pid;p->parent=t;p->owner=t->group;p->at=at;
 p->next=current_execution->processes;current_execution->processes=p;
 p->sibling=t->group->processes;t->group->processes=p;
 ++current_execution->process_count;++current_execution->process_active;return p;
}
static on1x_csp_process *owned_process(on1x_csp_process *value,on1x_csp_task *caller,source_point at){
 on1x_csp_process *p;
 for(p=current_execution->processes;p&&p!=value;p=p->next){}
 if(!p||p->joined||!p->owner||p->owner->owner!=caller||p->parent!=caller||p->joiner)failure(at,"invalid, repeated or non-owning external join/cancel");
 return p;
}
static int64_t finish_process(on1x_csp_process *p,on1x_csp_task *caller){
 p->joiner=caller->fiber;caller->waiting=true;
 while(!p->done)(void)mulib_fiber_park_until(MULIB_XWAIT_FOREVER);
 mulib_fiber_clear_wake();caller->waiting=false;p->joined=true;
 ++current_execution->progress;return p->status;
}
int64_t on1x_csp_process_join(on1x_csp_process *value,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *caller=current(at);on1x_csp_process *p;
 tick(at);p=owned_process(value,caller,at);reap(p);return finish_process(p,caller);
}
bool on1x_csp_process_cancel(on1x_csp_process *value,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *caller=current(at);on1x_csp_process *p;bool requested=false;
 tick(at);p=owned_process(value,caller,at);reap(p);
 if(!p->done){
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
 while(!t->done)(void)mulib_fiber_park_until(MULIB_XWAIT_FOREVER);
 mulib_fiber_clear_wake();caller->waiting=false;t->joined=true;
 ++current_execution->progress;return t->result;
}
on1x_csp_channel *on1x_csp_channel_create(int64_t capacity,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *t=current(at);on1x_csp_channel *c;
 tick(at);
 if(!t->group||capacity<0||capacity>256)failure(at,"invalid channel owner/capacity");
 if(current_execution->channel_count>=ON1X_CSP_MAX_CHANNELS)failure(at,"total channel budget exhausted");
 c=(on1x_csp_channel *)ON1X_CSP_CALLOC(1,sizeof(*c));if(!c)failure(at,"channel record allocation failure");
 if(mulib_channel_create(&c->channel,sizeof(int64_t),(size_t)capacity)!=MULIB_CHANNEL_OK)failure(at,"channel storage allocation failure");
 c->next=current_execution->channels;current_execution->channels=c;++current_execution->channel_count;
 c->sibling=t->group->channels;t->group->channels=c;return c;
}
int64_t on1x_csp_send(on1x_csp_channel *value,int64_t payload,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *t=current(at);on1x_csp_channel *c=channel(value,at);int rc;
 tick(at);t->waiting=true;
 rc=mulib_channel_send(mulib_channel_sender(c->channel),&payload,MULIB_XWAIT_FOREVER);
 t->waiting=false;++current_execution->progress;
 if(rc!=MULIB_CHANNEL_OK&&rc!=MULIB_CHANNEL_CLOSED)failure(at,"channel send allocation/protocol failure");
 return rc==MULIB_CHANNEL_CLOSED?1:0;
}
bool on1x_csp_receive(on1x_csp_channel *value,int64_t *payload,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *t=current(at);on1x_csp_channel *c=channel(value,at);int rc;
 if(!payload)failure(at,"invalid receive output");
 tick(at);t->waiting=true;
 rc=mulib_channel_receive(mulib_channel_receiver(c->channel),payload,MULIB_XWAIT_FOREVER);
 t->waiting=false;++current_execution->progress;
 if(rc!=MULIB_CHANNEL_OK&&rc!=MULIB_CHANNEL_CLOSED)failure(at,"channel receive allocation/protocol failure");
 return rc==MULIB_CHANNEL_OK;
}
bool on1x_csp_close(on1x_csp_channel *value,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_channel *c=channel(value,at);int rc;
 tick(at);rc=mulib_channel_close(c->channel);
 if(rc!=MULIB_CHANNEL_OK&&rc!=MULIB_CHANNEL_CLOSED)failure(at,"invalid close");
 return rc==MULIB_CHANNEL_OK;
}
void on1x_csp_select(on1x_csp_offer *offers,size_t count,bool has_default,int64_t *payload,size_t *selected,bool *closed,const char *file,size_t line,size_t column){
 source_point at={file,line,column};on1x_csp_task *t=current(at);
 mulib_channel_case_t cases[16];size_t i,index=SIZE_MAX;int rc;
 if(!offers||!payload||!selected||!closed||!count||count>16)failure(at,"invalid poll offers");
 tick(at);
 for(i=0;i<count;++i){
  on1x_csp_channel *c=channel(offers[i].channel,at);
  if(offers[i].operation==MULIB_CHANNEL_SEND)cases[i]=mulib_channel_send_case(mulib_channel_sender(c->channel),&offers[i].payload);
  else if(offers[i].operation==MULIB_CHANNEL_RECEIVE)cases[i]=mulib_channel_receive_case(mulib_channel_receiver(c->channel),&offers[i].payload);
  else failure(at,"invalid poll operation");
 }
 t->waiting=true;
 rc=mulib_channel_select(cases,count,has_default,MULIB_XWAIT_FOREVER,&index);
 t->waiting=false;++current_execution->progress;
 if(rc==MULIB_CHANNEL_DEFAULT&&has_default){*selected=count;*payload=0;*closed=false;return;}
 if((rc!=MULIB_CHANNEL_OK&&rc!=MULIB_CHANNEL_CLOSED)||index>=count)failure(at,"poll allocation/protocol failure");
 *selected=index;*closed=rc==MULIB_CHANNEL_CLOSED;*payload=rc==MULIB_CHANNEL_OK?offers[index].payload:0;
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
   on1x_csp_task *t,*blocked=NULL;on1x_csp_process *p;bool all_waiting=true;
   for(p=e->processes;p;p=p->next)if(!p->done){
    reap(p);
    if(!p->done&&p->stopping&&!p->killed&&monotonic_ms(p->at)>=p->kill_at){
     if(kill(-p->pid,SIGKILL)!=0&&errno!=ESRCH)failure(p->at,"external process-group SIGKILL failure");
     p->killed=true;
    }
   }
  for(t=e->tasks;t;t=t->next)if(!t->done){if(!t->waiting)all_waiting=false;else blocked=t;}
  /* A channel commit may have made a task runnable while its adapter still
   * reports waiting. Allow complete scheduler rounds for it to acknowledge.
   * This works only for the closed single-thread/no-external-I/O profile. */
   if(all_waiting&&previous==e->progress&&!e->process_active){if(++quiet>=2)failure(blocked?blocked->at:(source_point){"<scheduler>",1,1},"quiescent fiber/channel deadlock");}
   else quiet=0;
   previous=e->progress;
   if(all_waiting&&e->process_active)mulib_fiber_sleep(2);
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
 if(e.active||e.process_active||e.groups||!root->done)failure(at,"unfinished scheduler cleanup");
 result=root->result;
 if(mulib_fiber_scheduler_destroy(e.scheduler)!=MULIB_FIBER_OK)failure(at,"scheduler destruction failure");
 while((t=e.tasks)!=NULL){e.tasks=t->next;ON1X_CSP_FREE(t);}
 while((c=e.channels)!=NULL){e.channels=c->next;if(c->channel)failure(at,"unreleased channel");ON1X_CSP_FREE(c);}
 while((p=e.processes)!=NULL){e.processes=p->next;if(!p->done||!p->joined)failure(at,"unreaped external process");ON1X_CSP_FREE(p);}
 current_execution=NULL;return result;
}
