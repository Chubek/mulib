#include "../distrib-common.h"
int main(void) {
  const char config[]="cluster: demo\nnodes: [worker]\nlease_ms: 10000\n";
  const char tasks[]="distrib_task('answer','worker','mune','run','return 6 * 7')";
  mulib_distribtask_error_t error;
  mulib_distribtask_plan_t *p=mulib_distribtask_load(config,sizeof config-1,1,tasks,sizeof tasks-1,NULL,0,&error);
  mulib_mumq_server_t *server=mulib_mumq_server_create(NULL,NULL);
  mulib_socket_t a=MULIB_SOCKET_INIT,b=MULIB_SOCKET_INIT;
  mulib_mumq_t *coordinator, *worker, *peer;
  mulib_distribcomp_t *c;
  mulib_distribcomp_worker_t *w;
  mulib_distribcomp_worker_options_t options={0};
  mulib_distribcomp_task_status_t status;
  size_t slot;
  assert(p && server && !mulib_socket_pair(&a,&b));
  coordinator=mulib_mumq_create(&a,NULL); peer=mulib_mumq_create(&b,NULL);
  assert(coordinator && peer && !mulib_mumq_server_add(server,peer,&slot));
  assert(!mulib_socket_pair(&a,&b));
  worker=mulib_mumq_create(&a,NULL); peer=mulib_mumq_create(&b,NULL);
  assert(worker && peer && !mulib_mumq_server_add(server,peer,&slot));
  c=mulib_distribcomp_create(p,coordinator,"unique-demo-run");
  options.allow_mune=1; w=mulib_distribcomp_worker_create(worker,"demo","worker","unique-demo-run",&options);
  assert(c && w);
  for (uint64_t now=0;now<1000 && !mulib_distribcomp_finished(c);++now) {
    assert(mulib_distribcomp_step(c,now)>=0);
    assert(mulib_distribcomp_worker_step(w)>=0);
    assert(!mulib_mumq_server_step(server));
  }
  assert(mulib_distribcomp_finished(c)==1 && !mulib_distribcomp_status(c,0,&status));
  assert(!strcmp(status.output,"42")); puts(status.output);
  mulib_distribcomp_destroy(c); mulib_distribcomp_worker_destroy(w);
  mulib_mumq_destroy(coordinator); mulib_mumq_destroy(worker);
  mulib_mumq_server_destroy(server); mulib_distribtask_destroy(p); return 0;
}
