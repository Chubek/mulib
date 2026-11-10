#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
static int capture_fail,spawn_fail,wait_fail;
static FILE *capture(void){if(capture_fail&&!--capture_fail){errno=EMFILE;return NULL;}return tmpfile();}
static pid_t spawn(void){if(spawn_fail&&!--spawn_fail){errno=EAGAIN;return -1;}return fork();}
static pid_t reap(pid_t pid,int *raw,int options){
  if(wait_fail){wait_fail=0;errno=EINVAL;return -1;}return waitpid(pid,raw,options);
}
#define tmpfile capture
#define fork spawn
#define waitpid reap
#define MULIB_PARALLEL_SH_NO_MUNE
#define MULIB_IMPL_PARALLEL_SH
#include "../include/mulib-parallel-sh.h"
#undef tmpfile
#undef fork
#undef waitpid
#include <assert.h>

static void failure(int capture_at,int spawn_at,int wait_at,int retry){
  mulib_parallel_sh_t p;
  assert(!mulib_parallel_sh_init(&p,2));p.retries=retry;
  p.stdout_file=p.stderr_file=NULL;
  assert(!mulib_parallel_sh_add(&p,retry?"exit 1":"sleep 0.03"));
  if(!retry)assert(!mulib_parallel_sh_add(&p,"true"));
  capture_fail=capture_at;spawn_fail=spawn_at;wait_fail=wait_at;
  assert(mulib_parallel_sh_run(&p)==MULIB_PARALLEL_SH_SYSTEM);
  assert(p.last_status==MULIB_PARALLEL_SH_SYSTEM&&!p.active);
  for(size_t i=0;i<p.count;i++)assert(!p.jobs[i].pid&&!p.jobs[i].out&&!p.jobs[i].err);
  if(wait_at)assert(p.completed==2&&p.failed==2);
  mulib_parallel_sh_free(&p);capture_fail=spawn_fail=wait_fail=0;
}
int main(void){
  failure(1,0,0,0);failure(2,0,0,0);failure(3,0,0,0);
  failure(0,1,0,0);failure(0,2,0,0);failure(0,2,0,1);failure(0,0,1,0);
  /* An output error must survive both copying and buffered flushing. */
  {FILE *bad=fopen("/dev/null","r");mulib_parallel_sh_t p;assert(bad);
   assert(!mulib_parallel_sh_init(&p,1));p.stdout_file=bad;p.stderr_file=NULL;
   assert(!mulib_parallel_sh_add(&p,"printf data"));
   assert(mulib_parallel_sh_run(&p)==MULIB_PARALLEL_SH_SYSTEM);
   assert(p.completed==1&&!p.failed);mulib_parallel_sh_free(&p);fclose(bad);}
  {pid_t pid=fork();int raw;assert(pid>=0);
   if(!pid){mulib_parallel_sh_t p;FILE *f=tmpfile();char buf[8]={0};int rc;
     if(!f)_exit(1);
     close(0);close(1);close(2);
     mulib_parallel_sh_init(&p,1);p.stdout_file=f;p.stderr_file=NULL;
     mulib_parallel_sh_add(&p,"printf okay");rc=mulib_parallel_sh_run(&p);
     rewind(f);if(fread(buf,1,4,f)!=4)rc=1;
     mulib_parallel_sh_free(&p);fclose(f);_exit(rc||strcmp(buf,"okay")?1:0);
   }
   assert(waitpid(pid,&raw,0)==pid&&WIFEXITED(raw)&&!WEXITSTATUS(raw));}
  return 0;
}
