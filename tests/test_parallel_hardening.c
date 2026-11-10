#define MULIB_PARALLEL_SH_NO_MUNE
#define MULIB_IMPL_PARALLEL_SH
#include "../include/mulib-parallel-sh.h"
#include <assert.h>

static void expect(FILE *f,const char *text){
  char buf[1024];size_t n;
  rewind(f);n=fread(buf,1,sizeof buf-1,f);buf[n]=0;
  assert(!strcmp(buf,text));
}
static void *wait_closed(void *arg){
  char *s=(char*)1;
  assert(mulib_parallel_sh_channel_receive_wait(arg,&s,UINT64_MAX)==MULIB_PARALLEL_SH_CLOSED);
  assert(!s);return NULL;
}
static void channels(void){
  mulib_parallel_sh_t p;mulib_parallel_sh_channel_t *c,*again;
  pthread_t threads[4];char *s=(char*)1;
  assert(!mulib_parallel_sh_init(&p,1));
  assert(!mulib_parallel_sh_channel_create(&p,"queue",&c));
  assert(!mulib_parallel_sh_channel_create(&p,"queue",&again)&&again==c);
  assert(!mulib_parallel_sh_channel_set_limit(c,2));
  assert(!mulib_parallel_sh_channel_send(c,"one"));
  assert(!mulib_parallel_sh_channel_send(c,"two"));
  assert(mulib_parallel_sh_channel_set_limit(c,1)==MULIB_PARALLEL_SH_INVALID);
  assert(mulib_parallel_sh_channel_send(c,"three")==MULIB_PARALLEL_SH_FULL);
  assert(!mulib_parallel_sh_channel_receive(c,&s)&&!strcmp(s,"one"));free(s);
  assert(!mulib_parallel_sh_channel_send(c,"three"));
  assert(!mulib_parallel_sh_channel_receive(c,&s)&&!strcmp(s,"two"));free(s);
  assert(!mulib_parallel_sh_channel_receive(c,&s)&&!strcmp(s,"three"));free(s);
  assert(mulib_parallel_sh_channel_receive_wait(c,&s,2)==MULIB_PARALLEL_SH_EMPTY&&!s);
  for(size_t i=0;i<4;i++)assert(!pthread_create(&threads[i],NULL,wait_closed,c));
  assert(!mulib_parallel_sh_channel_close(c));
  for(size_t i=0;i<4;i++)assert(!pthread_join(threads[i],NULL));
  assert(mulib_parallel_sh_channel_send(c,"late")==MULIB_PARALLEL_SH_CLOSED);
  assert(!mulib_parallel_sh_channel_close(c));
  assert(!mulib_parallel_sh_channel_create(&p,"drain",&c));
  assert(!mulib_parallel_sh_channel_send(c,""));
  assert(!mulib_parallel_sh_channel_close(c));
  assert(!mulib_parallel_sh_channel_receive(c,&s)&&!*s);free(s);
  assert(mulib_parallel_sh_channel_receive(c,&s)==MULIB_PARALLEL_SH_CLOSED&&!s);
  mulib_parallel_sh_free(&p);
}
static void jobs(void){
  mulib_parallel_sh_t p;FILE *out=tmpfile(),*err=tmpfile(),*log=tmpfile();
  pid_t unrelated;int raw;
  assert(out&&err&&log);
  assert(!mulib_parallel_sh_init(&p,2));p.keep_order=1;
  p.stdout_file=out;p.stderr_file=err;p.joblog=log;
  unrelated=fork();assert(unrelated>=0);if(!unrelated)_exit(23);
  assert(!mulib_parallel_sh_add(&p,"sleep 0.04; printf first; printf err >&2"));
  assert(!mulib_parallel_sh_add(&p,"printf second"));
  assert(!mulib_parallel_sh_add_input(&p,"printf '%s' {}","' \" $HOME; `false`\n"));
  assert(!mulib_parallel_sh_run(&p));
  assert(p.completed==3&&!p.failed&&!p.active&&!p.last_status);
  expect(out,"firstsecond' \" $HOME; `false`\n");expect(err,"err");
  assert(waitpid(unrelated,&raw,0)==unrelated&&WEXITSTATUS(raw)==23);
  assert(mulib_parallel_sh_run(&p)==MULIB_PARALLEL_SH_INVALID);
  assert(mulib_parallel_sh_add(&p,"true")==MULIB_PARALLEL_SH_INVALID);
  for(size_t i=0;i<p.count;i++)assert(p.jobs[i].finished&&!p.jobs[i].pid&&!p.jobs[i].out&&!p.jobs[i].err);
  {char line[512];size_t lines=0;rewind(log);while(fgets(line,sizeof line,log))lines++;assert(lines==3);}
  mulib_parallel_sh_free(&p);fclose(out);fclose(err);fclose(log);

  assert(!mulib_parallel_sh_init(&p,1));p.retries=-1;
  assert(mulib_parallel_sh_run(&p)==MULIB_PARALLEL_SH_INVALID&&!p.ran);
  p.retries=2;p.halt_on_error=1;p.stdout_file=p.stderr_file=NULL;
  assert(!mulib_parallel_sh_add(&p,"exit 9"));
  assert(!mulib_parallel_sh_add(&p,"exit 0"));
  assert(mulib_parallel_sh_run(&p)==MULIB_PARALLEL_SH_FAILED);
  assert(p.completed==1&&p.failed==1&&p.jobs[0].attempts==3&&!p.jobs[1].attempts);
  mulib_parallel_sh_free(&p);
  assert(!mulib_parallel_sh_init(&p,1));p.stdout_file=p.stderr_file=NULL;
  assert(!mulib_parallel_sh_add(&p,"kill -TERM $$"));
  assert(mulib_parallel_sh_run(&p)==MULIB_PARALLEL_SH_FAILED);
  assert(p.jobs[0].term_signal==SIGTERM&&p.jobs[0].exit_code==128+SIGTERM);
  mulib_parallel_sh_free(&p);
}
typedef struct worker_counts {unsigned active,peak,started;} worker_counts;
static void locked_counts(int fd,worker_counts *counts,int delta){
  struct flock lock;memset(&lock,0,sizeof lock);lock.l_type=F_WRLCK;lock.l_whence=SEEK_SET;
  assert(!fcntl(fd,F_SETLKW,&lock));
  assert(pread(fd,counts,sizeof *counts,0)==sizeof *counts);
  if(delta>0){counts->active++;counts->started++;if(counts->active>counts->peak)counts->peak=counts->active;}
  if(delta<0){assert(counts->active);counts->active--;}
  assert(pwrite(fd,counts,sizeof *counts,0)==sizeof *counts);
  lock.l_type=F_UNLCK;assert(!fcntl(fd,F_SETLK,&lock));
}
static int worker(const char *path){
  int fd=open(path,O_RDWR);worker_counts counts;struct timespec delay={0,10000000};
  assert(fd>=0);locked_counts(fd,&counts,1);
  /* A first-wave barrier proves multiple jobs really overlap, rather than
   * relying on wall-clock speedups. The deadline turns serialization into failure. */
  for(unsigned i=0;counts.started<3&&i<200;i++){
    nanosleep(&delay,NULL);locked_counts(fd,&counts,0);
  }
  assert(counts.started>=3&&counts.peak<=3);
  nanosleep(&delay,NULL);locked_counts(fd,&counts,-1);close(fd);return 0;
}
static void concurrent_jobs(const char *program){
  char path[512];const char *dir=getenv("TMPDIR");int fd;
  worker_counts counts={0};mulib_parallel_sh_t p;char *pattern;
  if(!dir||!*dir)dir=access("/tmp/opencode",W_OK)==0?"/tmp/opencode":"/tmp";
  assert(snprintf(path,sizeof path,"%s/mulib-parallel-XXXXXX",dir)>0);
  fd=mkstemp(path);assert(fd>=0);
  assert(write(fd,&counts,sizeof counts)==sizeof counts);
  assert(!mulib_parallel_sh_init(&p,3));p.stdout_file=NULL;
  pattern=mulib_parallel_sh_expand("exec {} --worker",program,1);assert(pattern);
  for(int i=0;i<8;i++)assert(!mulib_parallel_sh_add_input(&p,pattern,path));
  free(pattern);assert(!mulib_parallel_sh_run(&p)&&p.completed==8);
  assert(pread(fd,&counts,sizeof counts,0)==sizeof counts);
  assert(counts.peak==3&&!counts.active&&counts.started==8);
  mulib_parallel_sh_free(&p);close(fd);

  assert(!mulib_parallel_sh_init(&p,1));p.retries=1;p.stdout_file=tmpfile();assert(p.stdout_file);
  assert(!mulib_parallel_sh_add_input(&p,
    "if test -e {}; then rm {}; printf discarded; exit 1; else printf recovered; fi",path));
  assert(!mulib_parallel_sh_run(&p)&&p.jobs[0].attempts==2&&!p.failed&&p.completed==1);
  expect(p.stdout_file,"recovered");fclose(p.stdout_file);mulib_parallel_sh_free(&p);
  assert(access(path,F_OK)<0&&errno==ENOENT);
}
int main(int argc,char **argv){
  if(argc==3&&!strcmp(argv[1],"--worker"))return worker(argv[2]);
  const char *patterns[]={"{","{/","{//","{.","{#","{/."};
  for(size_t i=0;i<sizeof patterns/sizeof *patterns;i++){
    char *s=mulib_parallel_sh_expand(patterns[i],"x",1);assert(s);free(s);
  }
  {char *s=mulib_parallel_sh_expand("{/.} {.}","dir/.profile",1);
   assert(s&&!strcmp(s,"'.profile' 'dir/.profile'"));free(s);}
  channels();jobs();concurrent_jobs(argv[0]);return 0;
}
