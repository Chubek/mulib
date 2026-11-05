#include "../examples/distrib-common.h"
static const char config[]="{\"cluster\":\"test\",\"nodes\":[\"a\",\"b\"],\"lease_ms\":100,\"attempts\":2}";
static mulib_distribtask_plan_t *load(const char *src) {
  mulib_distribtask_error_t e;
  mulib_distribtask_plan_t *p=mulib_distribtask_load(config,sizeof config-1,0,src,strlen(src),NULL,0,&e);
  if (!p) fprintf(stderr,"task: %s at %zu\n",e.message,e.offset);
  return p;
}
static void bad(const char *cfg, int format, const char *src) {
  mulib_distribtask_error_t e;
  mulib_distribtask_plan_t *p=mulib_distribtask_load(cfg,strlen(cfg),format,src,strlen(src),NULL,0,&e);
  assert(!p && e.code && e.message[0]);
}
static void parsing(void) {
  mulib_distribtask_plan_t *p=load("");
  assert(p&&!mulib_distribtask_count(p)); mulib_distribtask_destroy(p);
  bad("{}",0,""); bad("",0,""); bad("cluster: x\nnodes: [a,a]",1,"");
  bad("{\"cluster\":\"x\",\"nodes\":[\"a\"],\"attempts\":1.5}",0,"");
  bad("{\"cluster\":\"x\",\"nodes\":[\"a\"],\"lease_ms\":0}",0,"");
  bad(config,0,"distrib_task('x','missing','mune','run','return 1')");
  bad(config,0,"distrib_task('x','a','unknown','run','')");
  bad(config,0,"distrib_task('x','a')");
  bad(config,0,"distrib_task('x','a','mune','run','') distrib_task('x','a','mune','run','')");
  bad(config,0,"distrib_task('x','a','mune','run','') distrib_depends('x','missing')");
  bad(config,0,"distrib_task('x','a','mune','run','') distrib_depends('x','x')");
  bad(config,0,"distrib_task('x','a','mune','run','') distrib_task('y','b','mune','run','') distrib_depends('x','y') distrib_depends('y','x')");
  bad(config,0,"distrib_task('x','a','mune','run','') distrib_task('y','b','mune','run','') distrib_depends('x','y') distrib_depends('x','y')");
  bad(config,0,"distrib_task('x','a','mune','run','a' .. string.char(0))");
  bad(config,0,"distrib_task('x','a','mune','run',string.char(255))");
  bad(config,0,"for i=1,257 do distrib_task('x'..i,'a','mune','run','') end");
  p=load("distrib_depends('y','x') distrib_task('y','b','mune','run','') distrib_task('x','a','mune','run','')");
  assert(p&&mulib_distribtask_at(p,0)->dependencies[0]==1); mulib_distribtask_destroy(p);
  {
    char cp[4096],tp[4096],pp[4096]; const char *file=__FILE__,*slash=strrchr(file,'/');
    int prefix=slash?(int)(slash-file+1):0;
    (void)snprintf(cp,sizeof cp,"%.*s../library/ldistrib/cluster.yaml",prefix,file);
    (void)snprintf(tp,sizeof tp,"%.*s../library/ldistrib/example.distribtask",prefix,file);
    (void)snprintf(pp,sizeof pp,"%.*s../library/ldistrib/cluster.lua",prefix,file);
    p=mulib_distribtask_load_file(cp,1,tp,pp,NULL);
    assert(p&&mulib_distribtask_count(p)==4); mulib_distribtask_destroy(p);
    assert(!mulib_distribtask_load_file("/does-not-exist",0,tp,NULL,NULL));
  }
}
typedef struct fixture {
  mulib_distribtask_plan_t *plan;
  mulib_mumq_server_t *server;
  mulib_mumq_t *channels[3];
  mulib_distribcomp_t *coordinator;
  mulib_distribcomp_worker_t *workers[2];
  unsigned calls, failures;
} fixture;
static int execute(void *user, const char *kind, const char *action, const char *p, size_t n, mulib_strbuf_t *out) {
  fixture *f=(fixture *)user; (void)kind; (void)action;
  ++f->calls; assert(mulib_strbuf_append_n(out,p,n));
  if (f->failures) { --f->failures; return 7; } return 0;
}
static void init(fixture *f, const char *tasks) {
  mulib_distribcomp_worker_options_t o={0};
  memset(f,0,sizeof *f); f->plan=load(tasks); f->server=mulib_mumq_server_create(NULL,NULL);
  assert(f->plan&&f->server);
  for (size_t i=0;i<3;++i) {
    mulib_socket_t a=MULIB_SOCKET_INIT,b=MULIB_SOCKET_INIT; mulib_mumq_t *peer; size_t slot;
    mulib_mumq_options_t ao={0},bo={0}; unsigned char key[64]={0};
    ao.key=bo.key=key; ao.tx_prefix[0]=bo.rx_prefix[0]=(unsigned char)(i*2+1);
    ao.rx_prefix[0]=bo.tx_prefix[0]=(unsigned char)(i*2+2);
    assert(!mulib_socket_pair(&a,&b));
    f->channels[i]=mulib_mumq_create(&a,&ao); peer=mulib_mumq_create(&b,&bo);
    assert(f->channels[i]&&peer&&!mulib_mumq_server_add(f->server,peer,&slot));
  }
  f->coordinator=mulib_distribcomp_create(f->plan,f->channels[0],"run-1");
  o.execute=execute; o.user=f; o.allow_mune=1;
  f->workers[0]=mulib_distribcomp_worker_create(f->channels[1],"test","a","run-1",&o);
  f->workers[1]=mulib_distribcomp_worker_create(f->channels[2],"test","b","run-1",&o);
  assert(f->coordinator&&f->workers[0]&&f->workers[1]);
}
static void destroy(fixture *f) {
  mulib_distribcomp_destroy(f->coordinator);
  for (size_t i=0;i<2;++i) mulib_distribcomp_worker_destroy(f->workers[i]);
  for (size_t i=0;i<3;++i) mulib_mumq_destroy(f->channels[i]);
  mulib_mumq_server_destroy(f->server); mulib_distribtask_destroy(f->plan);
}
static void spin(fixture *f, uint64_t now) {
  for (unsigned i=0;i<30;++i) {
    assert(mulib_distribcomp_step(f->coordinator,now)>=0);
    for (size_t j=0;j<2;++j) assert(mulib_distribcomp_worker_step(f->workers[j])>=0);
    assert(!mulib_mumq_server_step(f->server));
  }
}
static void end_to_end(void) {
  fixture f; mulib_distribcomp_task_status_t s;
  init(&f,"distrib_task('one','a','container','start','first') distrib_task('two','b','mune','run','return 42') distrib_depends('two','one')");
  f.failures=1; spin(&f,0);
  assert(mulib_distribcomp_finished(f.coordinator)==1&&f.calls==2);
  assert(!mulib_distribcomp_status(f.coordinator,0,&s)&&s.attempt==2&&!strcmp(s.output,"first"));
  assert(!mulib_distribcomp_status(f.coordinator,1,&s)&&!strcmp(s.output,"42"));
  destroy(&f);
  init(&f,"distrib_task('one','a','container','start','first') distrib_task('two','b','mune','run','return 42') distrib_depends('two','one')");
  f.failures=100; spin(&f,0);
  assert(mulib_distribcomp_finished(f.coordinator)==-1&&f.calls==2);
  assert(!mulib_distribcomp_status(f.coordinator,1,&s)&&s.state==MULIB_DISTRIBCOMP_BLOCKED);
  destroy(&f);
}
static mulib_mumq_message_t message(const char *topic, const char *data) {
  mulib_mumq_message_t m={0}; m.type=1; strcpy(m.topic,topic);
  m.data=(unsigned char *)(void *)data; m.length=strlen(data); return m;
}
static void leases_and_duplicates(void) {
  fixture f; mulib_mumq_message_t m; mulib_distribcomp_task_status_t s;
  const char request[]="{\"v\":1,\"run\":\"run-1\",\"task\":\"x\",\"node\":\"a\",\"attempt\":1,\"kind\":\"container\",\"action\":\"start\",\"payload\":\"app\"}";
  const char reply[]="{\"v\":1,\"run\":\"run-1\",\"task\":\"x\",\"node\":\"a\",\"attempt\":1,\"code\":0,\"output\":\"old\"}";
  init(&f,"distrib_task('x','a','container','start','app')");
  /* Acknowledge subscriptions directly, then withhold actual worker execution. */
  m=message("cluster/test/result/run-1",""); m.type=MULIB_MUMQ_SUBACK;
  assert(!mulib_distribcomp_handle(f.coordinator,&m));
  assert(mulib_distribcomp_tick(f.coordinator,0)>=0);
  assert(mulib_distribcomp_tick(f.coordinator,100)>=0);
  assert(!mulib_distribcomp_status(f.coordinator,0,&s)&&s.attempt==2);
  m=message("cluster/test/result/run-1",reply);
  assert(mulib_distribcomp_handle(f.coordinator,&m)==1);
  assert(mulib_distribcomp_tick(f.coordinator,200)>=0);
  assert(mulib_distribcomp_finished(f.coordinator)==-1);
  assert(mulib_distribcomp_tick(f.coordinator,199)==-1);
  assert(mulib_distribcomp_tick(f.coordinator,UINT64_MAX)==-1);
  m=message("cluster/test/node/a",""); m.type=MULIB_MUMQ_SUBACK;
  assert(!mulib_distribcomp_worker_handle(f.workers[0],&m));
  m=message("cluster/test/node/a",request);
  assert(!mulib_distribcomp_worker_handle(f.workers[0],&m)&&f.calls==1);
  assert(!mulib_distribcomp_worker_handle(f.workers[0],&m)&&f.calls==1);
  {
    char newer[sizeof request], conflicting[sizeof request];
    strcpy(conflicting,request); strstr(conflicting,"app")[0]='X';
    m=message("cluster/test/node/a",conflicting);
    assert(mulib_distribcomp_worker_handle(f.workers[0],&m)==-1&&f.calls==1);
    strcpy(newer,request); strstr(newer,"\"attempt\":1")[10]='2';
    m=message("cluster/test/node/a",newer);
    assert(!mulib_distribcomp_worker_handle(f.workers[0],&m)&&f.calls==2);
    m=message("cluster/test/node/a",request);
    assert(mulib_distribcomp_worker_handle(f.workers[0],&m)==1&&f.calls==2);
  }
  m=message("cluster/test/node/a","{}"); assert(mulib_distribcomp_worker_handle(f.workers[0],&m)==-1);
  destroy(&f);
  init(&f,"distrib_task('x','a','mune','run','return 1')");
  assert(!mulib_distribcomp_cancel(f.coordinator)&&mulib_distribcomp_finished(f.coordinator)==-1);
  assert(!mulib_distribcomp_status(f.coordinator,0,&s)&&s.state==MULIB_DISTRIBCOMP_CANCELLED);
  destroy(&f);
}
static void daemons(void) {
  fixture f;
  init(&f,"distrib_task('start','a','daemon','start',[[daemon_name('svc') daemon_command('/bin/sleep','60')]]) "
          "distrib_task('stop','a','daemon','stop','svc') distrib_depends('stop','start')");
  f.workers[0]->options.allow_daemon=1;
  spin(&f,0); assert(mulib_distribcomp_finished(f.coordinator)==1);
  destroy(&f); /* Reaps/terminates the child even if it has not processed SIGTERM. */
}
static void capability_errors(void) {
  fixture f; mulib_distribcomp_task_status_t s;
  init(&f,"distrib_task('x','a','mune','run','return 1')");
  memset(&f.workers[0]->options,0,sizeof f.workers[0]->options);
  spin(&f,0); assert(mulib_distribcomp_finished(f.coordinator)==-1);
  assert(!mulib_distribcomp_status(f.coordinator,0,&s)&&s.code==-253);
  destroy(&f);
  init(&f,"distrib_task('x','a','mune','run','return string.char(255)')");
  spin(&f,0); assert(mulib_distribcomp_finished(f.coordinator)==-1);
  assert(!mulib_distribcomp_status(f.coordinator,0,&s)&&s.code==-255&&!strcmp(s.output,""));
  destroy(&f);
}
int main(void) { parsing(); end_to_end(); leases_and_duplicates(); daemons(); capability_errors(); return 0; }
