/* Generated-code-shaped C11 fixture: every suspend-live value belongs to the
 * frame. The scheduler stack only holds one invocation of resume at a time. */
#define _POSIX_C_SOURCE 200809L
#define MULIB_IMPL_CHANNEL
#define MULIB_IMPL_FIBER
#define MULIB_IMPL_XTHREADS
#include <mulib-channel.h>
#include "on1x_async_frame.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define AT __FILE__, __LINE__, 1
#define CHECK(test) do { if (!(test)) { \
 fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); \
 exit(1); \
} } while (0)

static bool real_clock;
static uint64_t now_ms;
static unsigned stacks, resumes, drops, owned_drops;
static on1x_csp_task *parent_child;

int __real_clock_gettime(clockid_t,struct timespec *);
int __wrap_clock_gettime(clockid_t clock,struct timespec *now){
 if(clock!=CLOCK_MONOTONIC||real_clock)return __real_clock_gettime(clock,now);
 now->tv_sec=(time_t)(now_ms/1000);
 now->tv_nsec=(long)((now_ms%1000)*1000000);
 return 0;
}
int __real_nanosleep(const struct timespec *,struct timespec *);
int __wrap_nanosleep(const struct timespec *delay,struct timespec *remaining){
 if(real_clock)return __real_nanosleep(delay,remaining);
 now_ms+=(uint64_t)delay->tv_sec*1000+(uint64_t)delay->tv_nsec/1000000;
 return 0;
}
int __real_mulib_fiber_spawn(mulib_fiber_scheduler_t *,mulib_fiber_fn,void *,size_t,mulib_fiber_t **);
int __wrap_mulib_fiber_spawn(mulib_fiber_scheduler_t *s,mulib_fiber_fn f,void *a,size_t n,mulib_fiber_t **out){
 ++stacks;
 return __real_mulib_fiber_spawn(s,f,a,n,out);
}

enum mode { NORMAL, EXPIRE_DURING_RESUME, BAD_ACTION, BAD_DELAY, BAD_BLOCKING, DEADLINE_TRACE };
enum pc { START, AFTER_TIMER, AFTER_YIELD };
struct frame {
 enum pc pc;
 enum mode mode;
 uint32_t multiplier;
 bool subtract;
 int64_t initial, delay;
 uint64_t earliest_resume;
 int64_t *owned; /* Initialized only by START. */
};

static uint64_t milliseconds(void){
 struct timespec now;
 CHECK(clock_gettime(CLOCK_MONOTONIC,&now)==0);
 return (uint64_t)now.tv_sec*1000+(uint64_t)now.tv_nsec/1000000;
}
static on1x_async_step resume(void *opaque){
 struct frame *f=opaque;
 ++resumes;
 switch(f->pc){
 case START:
  CHECK(!f->owned);
  if(f->mode==BAD_ACTION)return (on1x_async_step){0};
  if(f->mode==BAD_BLOCKING)on1x_csp_yield(AT);
  if(f->mode==BAD_DELAY)
   return (on1x_async_step){ON1X_ASYNC_SLEEP_MS,{.milliseconds=-1}};
  f->owned=malloc(sizeof(*f->owned));CHECK(f->owned);
  *f->owned=f->initial*(int64_t)f->multiplier;
  if(f->mode==EXPIRE_DURING_RESUME){
   now_ms+=5;
   return (on1x_async_step){ON1X_ASYNC_RETURN_I64,{.i64=99}};
  }
  f->earliest_resume=milliseconds()+(uint64_t)f->delay;
  f->pc=AFTER_TIMER;
  return (on1x_async_step){ON1X_ASYNC_SLEEP_MS,{.milliseconds=f->delay}};
 case AFTER_TIMER:
  CHECK(f->owned&&milliseconds()>=f->earliest_resume);
  *f->owned+=f->subtract?-3:3;
  f->pc=AFTER_YIELD;
  return (on1x_async_step){ON1X_ASYNC_YIELD,{.i64=0}};
 case AFTER_YIELD:
  CHECK(f->owned);
  return (on1x_async_step){ON1X_ASYNC_RETURN_I64,{.i64=*f->owned}};
 }
 abort();
}
static void drop(void *opaque){
 struct frame *f=opaque;
 ++drops;
 if(f->owned){++owned_drops;free(f->owned);}
 if(f->mode==DEADLINE_TRACE||f->mode==EXPIRE_DURING_RESUME){
  CHECK(resumes==1&&drops==1&&owned_drops==1);
  puts("dropped");
 }
 free(f);
}
static on1x_csp_task *launch(enum mode mode,int64_t initial,int64_t delay){
 struct frame *f=malloc(sizeof(*f));
 CHECK(f);
 *f=(struct frame){.mode=mode,.multiplier=3,.subtract=initial<0,.initial=initial,.delay=delay};
 return on1x_async_launch_v1(resume,drop,f,AT);
}
static void join(on1x_csp_task *t,int64_t value){
 on1x_async_result outcome=on1x_async_join_v1(t,AT);
 CHECK(outcome.status==ON1X_ASYNC_RETURNED&&outcome.i64==value);
}

static int64_t witness(on1x_env *unused){
 (void)unused;
 for(unsigned turn=0;resumes<2&&turn<4;++turn)on1x_csp_yield(AT);
 CHECK(resumes==2&&!drops);
 return 77;
}
static int64_t mixed(on1x_env *unused){
 (void)unused;
 on1x_csp_group *g=on1x_csp_group_enter(AT);
 on1x_csp_task *cancelled_before=launch(NORMAL,0,10);
 CHECK(!resumes&&!drops);
 CHECK(on1x_csp_cancel(cancelled_before,AT));
 CHECK(!resumes&&drops==1&&!owned_drops);
 drops=0;
 on1x_csp_task *finished=launch(NORMAL,13,10);
 on1x_csp_task *cancelled_while_waiting=launch(NORMAL,1,INT64_MAX);
 on1x_csp_task *fiber=on1x_csp_launch(witness,NULL,AT);
 CHECK(on1x_csp_join(fiber,AT)==77);
 CHECK(on1x_csp_cancel(cancelled_while_waiting,AT));
 CHECK(drops>=1&&owned_drops>=1);
 join(finished,42);
 CHECK(resumes==4&&drops==2&&owned_drops==2);
 on1x_csp_group_finish(g);
 CHECK(stacks==3); /* Owner, supervisor and witness only. */
 return 19;
}
static int64_t repeated(on1x_env *unused){
 (void)unused;
 for(int64_t n=1;n<=32;++n){
  on1x_csp_group *g=on1x_csp_group_enter(AT);
  on1x_csp_task *first=launch(NORMAL,n,n%2);
  on1x_csp_task *second=launch(NORMAL,-n,(n+1)%2);
  if(n%2==0){join(first,n*3+3);join(second,-n*3-3);}
  else {join(second,-n*3-3);join(first,n*3+3);}
  on1x_csp_group_finish(g);
 }
 CHECK(resumes==192&&drops==64&&owned_drops==64);
 CHECK(stacks==2);
 return 19;
}
static int64_t real_wait(on1x_env *unused){
 (void)unused;
 join(launch(NORMAL,13,3),42);
 CHECK(resumes==3&&drops==1&&owned_drops==1&&stacks==2);
 return 19;
}
static int64_t cancelled_parent(on1x_env *unused){
 (void)unused;
 on1x_csp_task *child=launch(NORMAL,13,100);
 CHECK(on1x_async_join_v1(child,AT).status==ON1X_ASYNC_JOIN_INTERRUPTED);
 return 0; /* The stackful parent's implicit taskgroup drains the frame. */
}
static int64_t nested_cancel(on1x_env *unused){
 (void)unused;
 on1x_csp_task *parent=on1x_csp_launch(cancelled_parent,NULL,AT);
 for(unsigned turn=0;!resumes&&turn<4;++turn)on1x_csp_yield(AT);
 CHECK(resumes==1&&!drops);
 CHECK(on1x_csp_cancel(parent,AT));
 CHECK(resumes==1&&drops==1&&owned_drops==1&&stacks==3);
 return 19;
}
static int64_t completed_cancel(on1x_env *unused){
 (void)unused;
 on1x_csp_task *first=launch(NORMAL,13,0);
 on1x_csp_task *second=launch(NORMAL,13,0);
 join(first,42);
 CHECK(!on1x_csp_cancel(second,AT));
 CHECK(resumes==6&&drops==2&&owned_drops==2&&stacks==2);
 return 19;
}
static int64_t expiry(enum mode mode){
 on1x_csp_group *g=on1x_csp_group_enter_within(5,"deadline",123,4);
 on1x_csp_task *t=launch(mode,0,100);
 CHECK(on1x_async_join_v1(t,AT).status==ON1X_ASYNC_JOIN_INTERRUPTED);
 on1x_csp_group_finish(g); /* Must consume the withdrawn wait's child. */
 CHECK(drops==1&&owned_drops==1&&resumes==1);
 puts("drained");
 return 0;
}
static int64_t expires_waiting(on1x_env *unused){(void)unused;return expiry(DEADLINE_TRACE);}
static int64_t expires_returning(on1x_env *unused){(void)unused;return expiry(EXPIRE_DURING_RESUME);}
static int64_t shielded(on1x_env *unused){
 (void)unused;
 on1x_csp_group *g=on1x_csp_group_enter_within(5,"deadline",123,4);
 on1x_csp_task *t=launch(DEADLINE_TRACE,0,100);
 on1x_csp_shield_enter();
 CHECK(on1x_async_join_v1(t,AT).status==ON1X_ASYNC_CANCELLED);
 join(launch(NORMAL,13,3),42);
 CHECK(resumes==4&&drops==2&&owned_drops==2);
 on1x_csp_shield_leave();
 on1x_csp_group_finish(g);
 puts("shield drained");
 return 0;
}
static int64_t malformed(on1x_env *unused){(void)unused;join(launch(BAD_ACTION,0,0),0);return 0;}
static int64_t negative(on1x_env *unused){(void)unused;join(launch(BAD_DELAY,0,0),0);return 0;}
static int64_t overflow(on1x_env *unused){
 (void)unused;
 now_ms=UINT64_MAX-1;
 join(launch(NORMAL,0,2),0);
 return 0;
}
static int64_t blocking(on1x_env *unused){(void)unused;join(launch(BAD_BLOCKING,0,0),0);return 0;}
static int64_t unjoined(on1x_env *unused){(void)unused;(void)launch(NORMAL,0,0);return 0;}
static int64_t wrong_kind(on1x_env *unused){(void)unused;return on1x_csp_join(launch(NORMAL,0,0),AT);}
static int64_t twice(on1x_env *unused){
 (void)unused;
 on1x_csp_task *child=launch(NORMAL,13,0);
 join(child,42);join(child,42);
 return 0;
}
static int64_t nonowner_join(on1x_env *unused){(void)unused;join(parent_child,42);return 0;}
static int64_t nonowner(on1x_env *unused){
 (void)unused;
 parent_child=launch(NORMAL,13,0);
 return on1x_csp_join(on1x_csp_launch(nonowner_join,NULL,AT),AT);
}

int main(int argc,char **argv){
 const struct {const char *name;on1x_csp_callback run;} cases[]={
  {"mixed",mixed},{"repeated",repeated},{"real",real_wait},
  {"nested-cancel",nested_cancel},{"completed-cancel",completed_cancel},
  {"deadline",expires_waiting},{"deadline-return",expires_returning},{"shielded",shielded},
  {"malformed",malformed},{"negative",negative},{"overflow",overflow},
  {"blocking",blocking},{"unjoined",unjoined},{"wrong-kind",wrong_kind},
  {"twice",twice},{"nonowner",nonowner}
 };
 CHECK(argc==2);
 real_clock=strcmp(argv[1],"real")==0;
 for(size_t i=0;i<sizeof(cases)/sizeof(*cases);++i)if(strcmp(cases[i].name,argv[1])==0){
  CHECK(on1x_csp_run_v5(cases[i].run,NULL,AT)==19);
  return 0;
 }
 return 1;
}
