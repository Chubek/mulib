#include "../distrib-common.h"
int main(void) {
  const char config[]="{\"cluster\":\"demo\",\"nodes\":[\"worker\"]}";
  const char tasks[]="distrib_task('answer','worker','mune','run','return 6 * 7')";
  mulib_distribtask_error_t error;
  mulib_distribtask_plan_t *p=mulib_distribtask_load(config,sizeof config-1,0,tasks,sizeof tasks-1,NULL,0,&error);
  assert(p && mulib_distribtask_count(p)==1);
  mulib_distribtask_destroy(p); return 0;
}
